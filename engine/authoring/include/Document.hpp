#pragma once

#include <Diagnostic.hpp>
#include <SceneFile.hpp>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace umbra::authoring
{
    // Keeps keys in file order, so saving doesn't reshuffle a hand-written file.
    using Json = nlohmann::ordered_json;

    // One change at a JSON pointer, e.g. "/entities/3/position".
    struct Change final
    {
        enum class Op
        {
            Set,    // replace the value (or add an object member)
            Insert, // insert into an array before the index ("-" = append)
            Remove
        };
        Op op = Op::Set;
        std::string path;
        Json value;
    };

    // Applies changes in order to a copy of `document` (no validation). False if a path is wrong.
    bool ApplyChanges(const Json &document, const std::vector<Change> &changes, Json &result, std::vector<core::Diagnostic> &diagnostics,
                      const std::string &name = "");

    // Checks a whole document; appends diagnostics, true if there are no errors.
    using Validator = std::function<bool(const Json &document, const std::string &name, std::vector<core::Diagnostic> &)>;
    // The game's own loaders, so the editor, the tool and the game always agree.
    [[nodiscard]] Validator SceneValidator(scene::SceneLoadOptions options);
    [[nodiscard]] Validator RpgValidator();

    // A content file being edited. Every edit is all-or-nothing: the changed document must pass
    // the validator, or nothing changes. Undo/redo restore whole earlier states. See Authoring.md.
    class Document final
    {
    public:
        Document(std::filesystem::path file, Validator validator) : file_(std::move(file)), validator_(std::move(validator)) {}

        // Reads and validates the file. On failure the document stays as it was.
        bool Open(std::vector<core::Diagnostic> &diagnostics);

        // Applies all changes or none. `expectedRevision`, when given, must equal Revision():
        // an edit prepared against an older state is a conflict and is rejected.
        // `merge` folds this edit into the previous undo step if that had the same label
        // (one undo step for a whole gizmo drag).
        bool Apply(const std::vector<Change> &changes, const std::string &label, std::vector<core::Diagnostic> &diagnostics,
                   std::optional<std::uint64_t> expectedRevision = std::nullopt, bool merge = false);
        bool Undo();
        bool Redo();
        [[nodiscard]] bool CanUndo() const noexcept { return !undo_.empty(); }
        [[nodiscard]] bool CanRedo() const noexcept { return !redo_.empty(); }
        [[nodiscard]] std::string UndoLabel() const { return undo_.empty() ? "" : undo_.back().label; }
        [[nodiscard]] std::string RedoLabel() const { return redo_.empty() ? "" : redo_.back().label; }

        // Writes atomically. Refuses (conflict) if the file was changed by someone else since it
        // was opened or last saved, unless `overwrite` is set.
        bool Save(std::vector<core::Diagnostic> &diagnostics, bool overwrite = false);
        [[nodiscard]] bool ChangedOnDisk() const;

        [[nodiscard]] const Json &Data() const noexcept { return data_; }
        [[nodiscard]] std::string Text() const { return data_.dump(2) + "\n"; }
        [[nodiscard]] const std::filesystem::path &File() const noexcept { return file_; }
        [[nodiscard]] bool IsOpen() const noexcept { return open_; }
        [[nodiscard]] bool IsDirty() const noexcept { return revision_ != savedRevision_; }
        // Names the current state: every edit makes a new number, undo/redo return to the number
        // that state had. So "dirty" and "conflict" are simple comparisons.
        [[nodiscard]] std::uint64_t Revision() const noexcept { return revision_; }

    private:
        struct Step final
        {
            std::string label;
            Json data;               // the whole document before (undo) or after (redo) the step
            std::uint64_t revision;  // the revision that state had
        };

        std::filesystem::path file_;
        Validator validator_;
        Json data_;
        bool open_ = false;
        std::uint64_t revision_ = 0;
        std::uint64_t savedRevision_ = 0;
        std::uint64_t nextRevision_ = 1;
        std::size_t diskHash_ = 0; // hash of the file text as last read or written
        std::vector<Step> undo_;
        std::vector<Step> redo_;
    };
}
