#include <Document.hpp>

#include <RpgContent.hpp>

#include <fstream>

namespace umbra::authoring
{
    namespace
    {
        constexpr std::size_t maxUndo = 200;

        bool ReadText(const std::filesystem::path &file, std::string &text)
        {
            std::ifstream in(file, std::ios::binary);
            if (!in)
                return false;
            text.assign(std::istreambuf_iterator<char>(in), {});
            return true;
        }

        void Error(std::vector<core::Diagnostic> &diagnostics, const std::string &code, const std::filesystem::path &file,
                   const std::string &field, const std::string &message)
        {
            diagnostics.push_back({core::Severity::Error, code, file.string(), field, message});
        }

        // Change -> one RFC 6902 JSON Patch operation.
        Json ToPatch(const Json &document, const Change &change)
        {
            switch (change.op)
            {
            case Change::Op::Set:
            {
                // "replace" needs an existing value; a new object member is an "add".
                const bool exists = document.contains(Json::json_pointer(change.path));
                return {{"op", exists ? "replace" : "add"}, {"path", change.path}, {"value", change.value}};
            }
            case Change::Op::Insert:
                return {{"op", "add"}, {"path", change.path}, {"value", change.value}};
            case Change::Op::Remove:
                break;
            }
            return {{"op", "remove"}, {"path", change.path}};
        }
    }

    Validator SceneValidator(scene::SceneLoadOptions options)
    {
        return [options](const Json &document, const std::string &name, std::vector<core::Diagnostic> &diagnostics) {
            return scene::ValidateSceneText(document.dump(), name, options, diagnostics);
        };
    }

    Validator RpgValidator()
    {
        return [](const Json &document, const std::string &name, std::vector<core::Diagnostic> &diagnostics) {
            rpg::Content content;
            return rpg::ParseContent(document.dump(), name, content, diagnostics);
        };
    }

    bool Document::Open(std::vector<core::Diagnostic> &diagnostics)
    {
        // 1. Read and parse.
        std::string text;
        if (!ReadText(file_, text))
        {
            Error(diagnostics, "file.read", file_, "", "cannot open file");
            return false;
        }
        Json data = Json::parse(text, nullptr, false);
        if (data.is_discarded())
        {
            Error(diagnostics, "json.parse", file_, "", "not valid JSON");
            return false;
        }
        // 2. Only a valid file is opened, so every later state is valid too.
        if (!validator_(data, file_.string(), diagnostics))
            return false;
        data_ = std::move(data);
        diskHash_ = std::hash<std::string>{}(text);
        undo_.clear();
        redo_.clear();
        revision_ = savedRevision_ = nextRevision_++;
        open_ = true;
        return true;
    }

    bool Document::Apply(const std::vector<Change> &changes, const std::string &label, std::vector<core::Diagnostic> &diagnostics,
                         std::optional<std::uint64_t> expectedRevision, bool merge)
    {
        if (!open_)
        {
            Error(diagnostics, "edit.closed", file_, "", "the document is not open");
            return false;
        }
        if (expectedRevision && *expectedRevision != revision_)
        {
            Error(diagnostics, "edit.conflict", file_, "", "the document changed since this edit was prepared");
            return false;
        }

        // 1. Apply to a copy, one change after another (later changes see earlier ones).
        Json next = data_;
        for (const Change &change : changes)
        {
            try
            {
                next = next.patch(Json::array({ToPatch(next, change)}));
            }
            catch (const Json::exception &error)
            {
                Error(diagnostics, "edit.path", file_, change.path, error.what());
                return false;
            }
        }
        // 2. The result must pass the same checks as loading; otherwise nothing changes.
        if (!validator_(next, file_.string(), diagnostics))
            return false;

        // 3. Commit, remembering the old state for undo (a merged step keeps its first "before").
        const bool fold = merge && !undo_.empty() && undo_.back().label == label;
        if (!fold)
        {
            undo_.push_back({label, std::move(data_), revision_});
            if (undo_.size() > maxUndo)
                undo_.erase(undo_.begin());
        }
        redo_.clear();
        data_ = std::move(next);
        revision_ = nextRevision_++;
        return true;
    }

    bool Document::Undo()
    {
        if (undo_.empty())
            return false;
        Step step = std::move(undo_.back());
        undo_.pop_back();
        redo_.push_back({step.label, std::move(data_), revision_});
        data_ = std::move(step.data);
        revision_ = step.revision;
        return true;
    }

    bool Document::Redo()
    {
        if (redo_.empty())
            return false;
        Step step = std::move(redo_.back());
        redo_.pop_back();
        undo_.push_back({step.label, std::move(data_), revision_});
        data_ = std::move(step.data);
        revision_ = step.revision;
        return true;
    }

    bool Document::ChangedOnDisk() const
    {
        std::string text;
        return open_ && (!ReadText(file_, text) || std::hash<std::string>{}(text) != diskHash_);
    }

    bool Document::Save(std::vector<core::Diagnostic> &diagnostics, bool overwrite)
    {
        if (!open_)
        {
            Error(diagnostics, "edit.closed", file_, "", "the document is not open");
            return false;
        }
        if (!overwrite && ChangedOnDisk())
        {
            Error(diagnostics, "save.conflict", file_, "", "the file was changed outside this editor; reload it or overwrite");
            return false;
        }
        // Atomic write: temp file, then rename over the target.
        const std::string text = Text();
        const std::filesystem::path temp = file_.string() + ".tmp";
        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            out << text;
            if (!out.flush())
            {
                Error(diagnostics, "save.write", file_, "", "cannot write " + temp.string());
                return false;
            }
        }
        std::error_code ec;
        std::filesystem::rename(temp, file_, ec);
        if (ec)
        {
            std::filesystem::remove(temp, ec);
            Error(diagnostics, "save.write", file_, "", "cannot replace the file");
            return false;
        }
        diskHash_ = std::hash<std::string>{}(text);
        savedRevision_ = revision_;
        return true;
    }
}
