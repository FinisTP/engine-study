#include "LlamaMind.hpp"

#include <llama.h>

#include <vector>

namespace umbra::town
{
    namespace
    {
        void Quiet(ggml_log_level, const char *, void *) {} // llama.cpp prints a lot; the game logs its own lines

        class LlamaMind final : public Mind
        {
        public:
            LlamaMind(llama_model *model, llama_context *context, std::string name, float temperature)
                : model_(model), context_(context), name_(std::move(name)), temperature_(temperature)
            {
            }
            ~LlamaMind() override
            {
                llama_free(context_);
                llama_model_free(model_);
            }
            std::string Name() const override { return name_; }

            std::string Complete(const MindRequest &request) override
            {
                const llama_vocab *vocab = llama_model_get_vocab(model_);
                // 1. The prompt in the model's own chat format (one user message, then its turn).
                std::string prompt = request.prompt;
                if (const char *chat = llama_model_chat_template(model_, nullptr))
                {
                    const llama_chat_message message{"user", request.prompt.c_str()};
                    std::vector<char> text(request.prompt.size() * 2 + 1024);
                    const int32_t length = llama_chat_apply_template(chat, &message, 1, true, text.data(), static_cast<int32_t>(text.size()));
                    if (length > 0 && static_cast<std::size_t>(length) <= text.size())
                        prompt.assign(text.data(), static_cast<std::size_t>(length));
                }
                // 2. To tokens.
                std::vector<llama_token> tokens(prompt.size() + 16);
                const int32_t count = llama_tokenize(vocab, prompt.c_str(), static_cast<int32_t>(prompt.size()), tokens.data(), static_cast<int32_t>(tokens.size()), true, true);
                if (count <= 0 || static_cast<uint32_t>(count + request.maxTokens) > llama_n_ctx(context_))
                    return {};
                tokens.resize(static_cast<std::size_t>(count));

                // 3. The sampler: the grammar first (only tokens that keep the answer valid), then a
                //    seeded draw, so one request always gives one answer.
                llama_sampler *sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
                if (!request.grammar.empty())
                {
                    llama_sampler *grammar = llama_sampler_init_grammar(vocab, request.grammar.c_str(), "root");
                    if (grammar == nullptr)
                    {
                        llama_sampler_free(sampler);
                        return {};
                    }
                    llama_sampler_chain_add(sampler, grammar);
                }
                llama_sampler_chain_add(sampler, llama_sampler_init_temp(temperature_));
                llama_sampler_chain_add(sampler, llama_sampler_init_dist(request.seed));

                // 4. Read the prompt, then write token by token until the grammar is done.
                llama_memory_clear(llama_get_memory(context_), true);
                std::string answer;
                llama_batch batch = llama_batch_get_one(tokens.data(), static_cast<int32_t>(tokens.size()));
                for (int written = 0; written < request.maxTokens; ++written)
                {
                    if (llama_decode(context_, batch) != 0)
                        break;
                    llama_token token = llama_sampler_sample(sampler, context_, -1);
                    if (llama_vocab_is_eog(vocab, token))
                        break;
                    char piece[128];
                    const int32_t size = llama_token_to_piece(vocab, token, piece, sizeof piece, 0, false);
                    if (size > 0)
                        answer.append(piece, static_cast<std::size_t>(size));
                    batch = llama_batch_get_one(&token, 1);
                }
                llama_sampler_free(sampler);
                return answer;
            }

        private:
            llama_model *model_;
            llama_context *context_;
            std::string name_;
            float temperature_;
        };
    }

    std::unique_ptr<Mind> LoadLlamaMind(const std::filesystem::path &file, const LlamaOptions &options, std::string &error)
    {
        std::error_code ignored;
        if (!std::filesystem::is_regular_file(file, ignored))
        {
            error = "no model file at " + file.generic_string();
            return nullptr;
        }
        llama_log_set(Quiet, nullptr);
        llama_backend_init();
        llama_model_params modelParams = llama_model_default_params();
        modelParams.n_gpu_layers = options.gpuLayers;
        llama_model *model = llama_model_load_from_file(file.string().c_str(), modelParams);
        if (model == nullptr)
        {
            error = "not a model llama.cpp can load: " + file.generic_string();
            return nullptr;
        }
        llama_context_params contextParams = llama_context_default_params();
        contextParams.n_ctx = static_cast<uint32_t>(options.context);
        contextParams.n_batch = static_cast<uint32_t>(options.context);
        contextParams.n_threads = options.threads;
        contextParams.n_threads_batch = options.threads;
        contextParams.no_perf = true;
        llama_context *context = llama_init_from_model(model, contextParams);
        if (context == nullptr)
        {
            llama_model_free(model);
            error = "cannot start the model (not enough memory?)";
            return nullptr;
        }
        return std::make_unique<LlamaMind>(model, context, file.filename().string(), options.temperature);
    }
}
