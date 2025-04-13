#include "run.h"
int GS = 0;

void error_usage()
{
    std::cerr << R"(Usage:   run <checkpoint> [options]
Example: run model.bin -n 256 -i "Once upon a time"
Options:
  -t <float>  temperature in [0,inf], default 1.0
  -p <float>  p value in top-p (nucleus) sampling in [0,1] default 0.9
  -s <int>    random seed, default time(NULL)
  -n <int>    number of steps to run for, default 256. 0 = max_seq_len
  -i <string> input prompt
  -z <string> optional path to custom tokenizer
  -m <string> mode: generate|chat, default: generate
  -y <string> (optional) system prompt in chat mode
)";
    std::exit(EXIT_FAILURE);
}

template<>
void Transformer<float>::malloc_weights()
{
    int head_size = config.dim / config.n_heads;
    // make sure the multiplications below are done in 64bit to fit the parameter counts of 13B+ models
    unsigned long long n_layers = config.n_layers;
    w.token_embedding_table = std::make_unique<float[]>(config.vocab_size * config.dim);
    w.rms_att_weight = std::make_unique<float[]>(n_layers * config.dim);
    w.wq = std::make_unique<float[]>(n_layers * config.dim * config.n_heads * head_size);
    w.wk = std::make_unique<float[]>(n_layers * config.dim * config.n_kv_heads * head_size);
    w.wv = std::make_unique<float[]>(n_layers * config.dim * config.n_kv_heads * head_size);
    w.wo = std::make_unique<float[]>(n_layers * config.dim * config.n_heads * head_size);
    w.rms_ffn_weight = std::make_unique<float[]>(n_layers * config.dim);
    w.w1 = std::make_unique<float[]>(n_layers * config.dim * config.hidden_dim);
    w.w2 = std::make_unique<float[]>(n_layers * config.dim * config.hidden_dim);
    w.w3 = std::make_unique<float[]>(n_layers * config.dim * config.hidden_dim);
    w.rms_final_weight = std::make_unique<float[]>(config.dim);
    if (!shared_weights)
    {
        w.wcls = std::make_unique<float[]>(config.vocab_size * config.dim);
        if (!w.wcls.get())
        {
            std::cerr << "Malloc for wcls weights failed.\n";
            std::exit(EXIT_FAILURE);
        }
    }
    if (!w.token_embedding_table.get() || !w.rms_att_weight.get() || !w.wq.get() || !w.wk.get() || !w.wv.get() || !w.wo.get() || !w.rms_ffn_weight.get() || !w.w1.get() ||
        !w.w2.get() || !w.w3.get() || !w.rms_final_weight.get())
    {
        std::cerr << "Malloc for weights failed.\n";
        std::exit(EXIT_FAILURE);
    }
}

template<>
void Transformer<QuantizedTensor>::malloc_weights()
{
    // int head_size = config.dim / config.n_heads;
    // make sure the multiplications below are done in 64bit to fit the parameter counts of 13B+ models
    unsigned long long n_layers = config.n_layers;
    w.token_embedding_table = std::make_unique<float[]>(config.vocab_size * config.dim);
    w.rms_att_weight = std::make_unique<float[]>(n_layers * config.dim);
    w.wq = std::make_unique<QuantizedTensor[]>(n_layers);
    w.wk = std::make_unique<QuantizedTensor[]>(n_layers);
    w.wv = std::make_unique<QuantizedTensor[]>(n_layers);
    w.wo = std::make_unique<QuantizedTensor[]>(n_layers);
    w.rms_ffn_weight = std::make_unique<float[]>(n_layers * config.dim);
    w.w1 = std::make_unique<QuantizedTensor[]>(n_layers);
    w.w2 = std::make_unique<QuantizedTensor[]>(n_layers);
    w.w3 = std::make_unique<QuantizedTensor[]>(n_layers);
    w.rms_final_weight = std::make_unique<float[]>(config.dim);

    w.q_tokens = std::make_unique<QuantizedTensor[]>(1);

    if (!shared_weights)
    {
        w.wcls = std::make_unique<QuantizedTensor[]>(1);
        if (!w.wcls.get())
        {
            std::cerr << "Malloc for wcls weights failed.\n";
            std::exit(EXIT_FAILURE);
        }
    }
    if (!w.token_embedding_table.get() || !w.rms_att_weight.get() || !w.wq.get() || !w.q_tokens.get() || !w.wk.get() || !w.wv.get() || !w.wo.get() ||
        !w.rms_ffn_weight.get() || !w.w1.get() || !w.w2.get() || !w.w3.get() || !w.rms_final_weight.get())
    {
        std::cerr << "Malloc for weights failed.\n";
        std::exit(EXIT_FAILURE);
    }
}

template<>
void Transformer<float>::malloc_run_state()
{
    int kv_dim = (config.dim * config.n_kv_heads) / config.n_heads;
    s.x = std::make_unique<float[]>(config.dim);
    s.xb = std::make_unique<float[]>(config.dim);
    s.xb2 = std::make_unique<float[]>(config.dim);
    s.hb = std::make_unique<float[]>(config.hidden_dim);
    s.hb2 = std::make_unique<float[]>(config.hidden_dim);
    s.q = std::make_unique<float[]>(config.dim);
    s.k = std::make_unique<float[]>(kv_dim);
    s.v = std::make_unique<float[]>(kv_dim);
    s.att = std::make_unique<float[]>(config.seq_len * config.n_heads);
    s.logits = std::make_unique<float[]>(config.vocab_size);
    s.key_cache = std::make_unique<float[]>(config.n_layers * config.seq_len * kv_dim);
    s.value_cache = std::make_unique<float[]>(config.n_layers * config.seq_len * kv_dim);
    if (!s.x.get() || !s.xb.get() || !s.xb2.get() || !s.hb.get() || !s.hb2.get() || !s.q.get() || !s.k.get() || !s.v.get() || !s.att.get() || !s.logits.get() ||
        !s.key_cache.get() || !s.value_cache.get())
    {
        std::cerr << "Malloc for run state failed.\n";
        std::exit(EXIT_FAILURE);
    }
}

template<>
void Transformer<QuantizedTensor>::malloc_run_state()
{
    int kv_dim = (config.dim * config.n_kv_heads) / config.n_heads;
    s.x = std::make_unique<float[]>(config.dim);
    s.xb = std::make_unique<float[]>(config.dim);
    s.xb2 = std::make_unique<float[]>(config.dim);
    s.hb = std::make_unique<float[]>(config.hidden_dim);
    s.hb2 = std::make_unique<float[]>(config.hidden_dim);
    s.q = std::make_unique<float[]>(config.dim);
    s.k = std::make_unique<float[]>(kv_dim);
    s.v = std::make_unique<float[]>(kv_dim);
    s.att = std::make_unique<float[]>(config.seq_len * config.n_heads);
    s.logits = std::make_unique<float[]>(config.vocab_size);
    s.key_cache = std::make_unique<float[]>(config.n_layers * config.seq_len * kv_dim);
    s.value_cache = std::make_unique<float[]>(config.n_layers * config.seq_len * kv_dim);
    if (!s.x.get() || !s.xb.get() || !s.xb2.get() || !s.hb.get() || !s.hb2.get() || !s.q.get() || !s.k.get() || !s.v.get() || !s.att.get() || !s.logits.get() ||
        !s.key_cache.get() || !s.value_cache.get())
    {
        std::cerr << "Malloc for run state failed.\n";
        std::exit(EXIT_FAILURE);
    }

    s.xq = std::make_unique<QuantizedTensor[]>(1);
    s.hq = std::make_unique<QuantizedTensor[]>(1);
    if (!s.xq.get() || !s.hq.get())
    {
        std::cerr << "Malloc for run state xq or hq failed.\n";
        std::exit(EXIT_FAILURE);
    }
    s.xq[0].q = std::make_unique<int8_t[]>(config.dim);
    s.xq[0].s = std::make_unique<float[]>(config.dim);
    if (!s.xq[0].q.get() || !s.xq[0].s.get())
    {
        std::cerr << "Malloc for run state xq[0] failed.\n";
        std::exit(EXIT_FAILURE);
    }
    s.hq[0].q = std::make_unique<int8_t[]>(config.hidden_dim);
    s.hq[0].s = std::make_unique<float[]>(config.hidden_dim);
    if (!s.hq[0].q.get() || !s.hq[0].s.get())
    {
        std::cerr << "Malloc for run state hq[0] failed.\n";
        std::exit(EXIT_FAILURE);
    }
}

template<>
void Transformer<float>::load_model(const std::string &checkpoint_path)
{
    std::ifstream file(checkpoint_path, std::ios::binary);
    if (!file)
    {
        std::cerr << "Couldn't open file " << checkpoint_path << '\n';
        std::exit(EXIT_FAILURE);
    }
    // 60816028 bytes
    file.read(reinterpret_cast<char *>(&config), sizeof(Config));
    shared_weights = config.vocab_size > 0 ? 1 : 0;
    config.vocab_size = std::abs(config.vocab_size);
    malloc_weights();
    int head_size = config.dim / config.n_heads;
    unsigned long long n_layers = config.n_layers;

    file.read(reinterpret_cast<char *>(w.token_embedding_table.get()), config.vocab_size * config.dim * sizeof(float));
    file.read(reinterpret_cast<char *>(w.rms_att_weight.get()), config.n_layers * config.dim * sizeof(float));
    file.read(reinterpret_cast<char *>(w.wq.get()), n_layers * config.dim * config.n_heads * head_size * sizeof(float));
    file.read(reinterpret_cast<char *>(w.wk.get()), n_layers * config.dim * config.n_kv_heads * head_size * sizeof(float));
    file.read(reinterpret_cast<char *>(w.wv.get()), n_layers * config.dim * config.n_kv_heads * head_size * sizeof(float));
    file.read(reinterpret_cast<char *>(w.wo.get()), n_layers * config.dim * config.n_heads * head_size * sizeof(float));
    file.read(reinterpret_cast<char *>(w.rms_ffn_weight.get()), n_layers * config.dim * sizeof(float));
    file.read(reinterpret_cast<char *>(w.w1.get()), n_layers * config.dim * config.hidden_dim * sizeof(float));
    file.read(reinterpret_cast<char *>(w.w2.get()), n_layers * config.dim * config.hidden_dim * sizeof(float));
    file.read(reinterpret_cast<char *>(w.w3.get()), n_layers * config.dim * config.hidden_dim * sizeof(float));
    file.read(reinterpret_cast<char *>(w.rms_final_weight.get()), config.dim * sizeof(float));

    if (!shared_weights)
    {
        file.seekg((config.seq_len * head_size) * sizeof(float), std::ios::cur);
        file.read(reinterpret_cast<char *>(w.wcls.get()), config.vocab_size * config.dim * sizeof(float));
    }
    file.close();
    malloc_run_state();
}

template<>
void Transformer<QuantizedTensor>::load_model(const std::string &checkpoint_path)
{

    std::ifstream file(checkpoint_path, std::ios::binary);
    if (!file)
    {
        std::cerr << "Couldn't open file " << checkpoint_path << '\n';
        std::exit(EXIT_FAILURE);
    }

    uint32_t magic_number;
    file.read(reinterpret_cast<char *>(&magic_number), sizeof(uint32_t));
    if (magic_number != 0x616b3432)
    {
        std::cerr << "Bad magic number\n";
        std::exit(EXIT_FAILURE);
    }

    int version;
    file.read(reinterpret_cast<char *>(&version), sizeof(int));
    if (version != 2)
    {
        std::cerr << "Bad version " << version << ", need version 2\n";
        std::exit(EXIT_FAILURE);
    }

    file.read(reinterpret_cast<char *>(&config), sizeof(Config));

    // read in flags
    uint8_t shared_classifier; // a byte to indicate if the classifier is shared
    file.read(reinterpret_cast<char *>(&shared_classifier), sizeof(uint8_t));

    int group_size;
    file.read(reinterpret_cast<char *>(&group_size), sizeof(int));
    GS = group_size;

    shared_weights = shared_classifier;
    // config.vocab_size = std::abs(config.vocab_size);

    malloc_weights();
    int head_size = config.dim / config.n_heads;
    unsigned long long n_layers = config.n_layers;

    int header_size = 256;
    file.seekg(header_size, std::ios::beg);
    file.read(reinterpret_cast<char *>(w.rms_att_weight.get()), config.n_layers * config.dim * sizeof(float));
    file.read(reinterpret_cast<char *>(w.rms_ffn_weight.get()), n_layers * config.dim * sizeof(float));
    file.read(reinterpret_cast<char *>(w.rms_final_weight.get()), config.dim * sizeof(float));
    init_quantized_tensors(file, w.q_tokens.get(), 1, config.vocab_size * config.dim);
    dequantize(w.q_tokens.get(), w.token_embedding_table.get(), config.vocab_size * config.dim);

    init_quantized_tensors(file, w.wq.get(), n_layers, config.dim * config.n_heads * head_size);
    init_quantized_tensors(file, w.wk.get(), n_layers, config.dim * config.n_kv_heads * head_size);
    init_quantized_tensors(file, w.wv.get(), n_layers, config.dim * config.n_kv_heads * head_size);
    init_quantized_tensors(file, w.wo.get(), n_layers, config.dim * config.n_heads * head_size);

    init_quantized_tensors(file, w.w1.get(), n_layers, config.dim * config.hidden_dim);
    init_quantized_tensors(file, w.w2.get(), n_layers, config.dim * config.hidden_dim);
    init_quantized_tensors(file, w.w3.get(), n_layers, config.dim * config.hidden_dim);

    if (!shared_weights)
    {
        init_quantized_tensors(file, w.wcls.get(), 1, config.dim * config.vocab_size);
    }
    file.close();
    malloc_run_state();
}

template<>
float *Transformer<float>::forward(int token, int pos)
{
    int dim = config.dim;
    int kv_dim = (config.dim * config.n_kv_heads) / config.n_heads;
    int kv_mul = config.n_heads / config.n_kv_heads; // integer multiplier of the kv sharing in multiquery
    int hidden_dim = config.hidden_dim;
    int head_size = dim / config.n_heads;
    // copy the token embedding into x
    std::memcpy(s.x.get(), w.token_embedding_table.get() + token * dim, dim * sizeof(*(s.x.get())));
    // forward all the layers
    for (decltype(config.n_layers) l = 0; l < config.n_layers; l++)
    {
        // attention rmsnorm
        rmsnorm(s.xb.get(), s.x.get(), w.rms_att_weight.get() + l * dim, dim);

        // qkv matmuls for this position
        matmul(s.q.get(), s.xb.get(), w.wq.get() + l * dim * dim, dim, dim);
        matmul(s.k.get(), s.xb.get(), w.wk.get() + l * dim * kv_dim, dim, kv_dim);
        matmul(s.v.get(), s.xb.get(), w.wv.get() + l * dim * kv_dim, dim, kv_dim);

        // RoPE relative positional encoding: complex-valued rotate q and k in each head
        for (int i = 0; i < dim; i += 2)
        {
            int head_dim = i % head_size;
            float freq = 1.0f / powf(10000.0f, head_dim / (float) head_size);
            float val = pos * freq;
            float fcr = cosf(val);
            float fci = sinf(val);
            int rotn = i < kv_dim ? 2 : 1; // how many vectors? 2 = q & k, 1 = q only
            for (int v = 0; v < rotn; v++)
            {
                float *vec = v == 0 ? s.q.get() : s.k.get(); // the vector to rotate (query or key)
                float v0 = vec[i];
                float v1 = vec[i + 1];
                vec[i] = v0 * fcr - v1 * fci;
                vec[i + 1] = v0 * fci + v1 * fcr;
            }
        }

        // save key,value at this time step (pos) to our kv cache
        int loff = l * config.seq_len * kv_dim; // kv cache layer offset for convenience
        float *key_cache_row = s.key_cache.get() + loff + pos * kv_dim;
        float *value_cache_row = s.value_cache.get() + loff + pos * kv_dim;
        std::memcpy(key_cache_row, s.k.get(), kv_dim * sizeof(*key_cache_row));
        std::memcpy(value_cache_row, s.v.get(), kv_dim * sizeof(*value_cache_row));

        // multihead attention. iterate over all heads
        int h;
        for (h = 0; h < config.n_heads; h++)
        {
            // get the query vector for this head
            float *q = s.q.get() + h * head_size;
            // attention scores for this head
            float *att = s.att.get() + h * config.seq_len;
            // iterate over all timesteps, including the current one
            for (int t = 0; t <= pos; t++)
            {
                // get the key vector for this head and at this timestep
                float *k = s.key_cache.get() + loff + t * kv_dim + (h / kv_mul) * head_size;
                // calculate the attention score as the dot product of q and k
                float score = 0.0f;
                for (int i = 0; i < head_size; i++)
                {
                    // q.shape = (1,head_size) k.shape= (head_size, n_head, seq_len)
                    score += q[i] * k[i];
                }
                score /= sqrtf(head_size);
                // save the score to the attention buffer
                // att.shape = (n_heads, seq_len)
                att[t] = score;
            }

            // softmax the scores to get attention weights, from 0..pos inclusively
            softmax(att, pos + 1);

            // weighted sum of the values, store back into xb
            float *xb = s.xb.get() + h * head_size;
            std::memset(xb, 0, head_size * sizeof(float));
            for (int t = 0; t <= pos; t++)
            {
                // get the value vector for this head and at this timestep
                float *v = s.value_cache.get() + loff + t * kv_dim + (h / kv_mul) * head_size;
                // get the attention weight for this timestep
                float a = att[t];
                // accumulate the weighted value into xb
                for (int i = 0; i < head_size; i++)
                {
                    xb[i] += a * v[i];
                }
            }
        }

        // final matmul to get the output of the attention
        matmul(s.xb2.get(), s.xb.get(), w.wo.get() + l * dim * dim, dim, dim);

        // residual connection back into x
        for (int i = 0; i < dim; i++)
        {
            s.x[i] += s.xb2[i];
        }

        // ffn rmsnorm
        rmsnorm(s.xb.get(), s.x.get(), w.rms_ffn_weight.get() + l * dim, dim);

        // Now for FFN in PyTorch we have: self.w2(F.silu(self.w1(x)) * self.w3(x))
        // first calculate self.w1(x) and self.w3(x)
        matmul(s.hb.get(), s.xb.get(), w.w1.get() + l * dim * hidden_dim, dim, hidden_dim);
        matmul(s.hb2.get(), s.xb.get(), w.w3.get() + l * dim * hidden_dim, dim, hidden_dim);

        // SwiGLU non-linearity
        for (int i = 0; i < hidden_dim; i++)
        {
            float val = s.hb[i];
            // silu(x)=x*σ(x), where σ(x) is the logistic sigmoid
            val *= (1.0f / (1.0f + expf(-val)));
            // elementwise multiply with w3(x)
            val *= s.hb2[i];
            s.hb[i] = val;
        }

        // final matmul to get the output of the ffn
        matmul(s.xb.get(), s.hb.get(), w.w2.get() + l * dim * hidden_dim, hidden_dim, dim);

        // residual connection
        for (int i = 0; i < dim; i++)
        {
            s.x[i] += s.xb[i];
        }
    }
    // final rmsnorm
    rmsnorm(s.x.get(), s.x.get(), w.rms_final_weight.get(), dim);
    // classifier into logits

    if (shared_weights)
    {
        // w.wcls = std::move(w.token_embedding_table);
        matmul(s.logits.get(), s.x.get(), w.token_embedding_table.get(), config.dim, config.vocab_size);
    }
    else
    {
        matmul(s.logits.get(), s.x.get(), w.wcls.get(), config.dim, config.vocab_size);
    }
    return s.logits.get();
}

template<>
float *Transformer<QuantizedTensor>::forward(int token, int pos)
{
    int dim = config.dim;
    int kv_dim = (config.dim * config.n_kv_heads) / config.n_heads;
    int kv_mul = config.n_heads / config.n_kv_heads; // integer multiplier of the kv sharing in multiquery
    int hidden_dim = config.hidden_dim;
    int head_size = dim / config.n_heads;
    // copy the token embedding into x
    std::memcpy(s.x.get(), w.token_embedding_table.get() + token * dim, dim * sizeof(float));
    // forward all the layers
    for (decltype(config.n_layers) l = 0; l < config.n_layers; l++)
    {
        // attention rmsnorm
        rmsnorm(s.xb.get(), s.x.get(), w.rms_att_weight.get() + l * dim, dim);

        // qkv matmuls for this position
        quantize(s.xq.get(), s.xb.get(), dim);
        q_matmul(s.q.get(), s.xq.get(), w.wq.get() + l, dim, dim);
        q_matmul(s.k.get(), s.xq.get(), w.wk.get() + l, dim, kv_dim);
        q_matmul(s.v.get(), s.xq.get(), w.wv.get() + l, dim, kv_dim);

        // RoPE relative positional encoding: complex-valued rotate q and k in each head
        for (int i = 0; i < dim; i += 2)
        {
            int head_dim = i % head_size;
            float freq = 1.0f / powf(10000.0f, head_dim / (float) head_size);
            float val = pos * freq;
            float fcr = cosf(val);
            float fci = sinf(val);
            int rotn = i < kv_dim ? 2 : 1; // how many vectors? 2 = q & k, 1 = q only
            for (int v = 0; v < rotn; v++)
            {
                float *vec = v == 0 ? s.q.get() : s.k.get(); // the vector to rotate (query or key)
                float v0 = vec[i];
                float v1 = vec[i + 1];
                vec[i] = v0 * fcr - v1 * fci;
                vec[i + 1] = v0 * fci + v1 * fcr;
            }
        }

        // save key,value at this time step (pos) to our kv cache
        int loff = l * config.seq_len * kv_dim; // kv cache layer offset for convenience
        float *key_cache_row = s.key_cache.get() + loff + pos * kv_dim;
        float *value_cache_row = s.value_cache.get() + loff + pos * kv_dim;
        std::memcpy(key_cache_row, s.k.get(), kv_dim * sizeof(*key_cache_row));
        std::memcpy(value_cache_row, s.v.get(), kv_dim * sizeof(*value_cache_row));

        // multihead attention. iterate over all heads
        int h;
        for (h = 0; h < config.n_heads; h++)
        {
            // get the query vector for this head
            float *q = s.q.get() + h * head_size;
            // attention scores for this head
            float *att = s.att.get() + h * config.seq_len;
            // iterate over all timesteps, including the current one
            for (int t = 0; t <= pos; t++)
            {
                // get the key vector for this head and at this timestep
                float *k = s.key_cache.get() + loff + t * kv_dim + (h / kv_mul) * head_size;
                // calculate the attention score as the dot product of q and k
                float score = 0.0f;
                for (int i = 0; i < head_size; i++)
                {
                    // q.shape = (1,head_size) k.shape= (head_size, n_head, seq_len)
                    score += q[i] * k[i];
                }
                score /= sqrtf(head_size);
                // save the score to the attention buffer
                // att.shape = (n_heads, seq_len)
                att[t] = score;
            }

            // softmax the scores to get attention weights, from 0..pos inclusively
            softmax(att, pos + 1);

            // weighted sum of the values, store back into xb
            float *xb = s.xb.get() + h * head_size;
            std::memset(xb, 0, head_size * sizeof(float));
            for (int t = 0; t <= pos; t++)
            {
                // get the value vector for this head and at this timestep
                float *v = s.value_cache.get() + loff + t * kv_dim + (h / kv_mul) * head_size;
                // get the attention weight for this timestep
                float a = att[t];
                // accumulate the weighted value into xb
                for (int i = 0; i < head_size; i++)
                {
                    xb[i] += a * v[i];
                }
            }
        }

        quantize(s.xq.get(), s.xb.get(), dim);
        // final matmul to get the output of the attention
        q_matmul(s.xb2.get(), s.xq.get(), w.wo.get() + l, dim, dim);

        // residual connection back into x
        for (int i = 0; i < dim; i++)
        {
            s.x[i] += s.xb2[i];
        }

        // ffn rmsnorm
        rmsnorm(s.xb.get(), s.x.get(), w.rms_ffn_weight.get() + l * dim, dim);

        // Now for FFN in PyTorch we have: self.w2(F.silu(self.w1(x)) * self.w3(x))
        // first calculate self.w1(x) and self.w3(x)
        quantize(s.xq.get(), s.xb.get(), dim);
        q_matmul(s.hb.get(), s.xq.get(), w.w1.get() + l, dim, hidden_dim);
        q_matmul(s.hb2.get(), s.xq.get(), w.w3.get() + l, dim, hidden_dim);

        // SwiGLU non-linearity
        for (int i = 0; i < hidden_dim; i++)
        {
            float val = s.hb[i];
            // silu(x)=x*σ(x), where σ(x) is the logistic sigmoid
            val *= (1.0f / (1.0f + expf(-val)));
            // elementwise multiply with w3(x)
            val *= s.hb2[i];
            s.hb[i] = val;
        }

        // final matmul to get the output of the ffn
        quantize(s.hq.get(), s.hb.get(), hidden_dim);
        q_matmul(s.xb.get(), s.hq.get(), w.w2.get() + l, hidden_dim, dim);

        // residual connection
        for (int i = 0; i < dim; i++)
        {
            s.x[i] += s.xb[i];
        }
    }
    // final rmsnorm
    rmsnorm(s.x.get(), s.x.get(), w.rms_final_weight.get(), dim);
    // classifier into logits

    quantize(s.xq.get(), s.x.get(), dim);
    if (shared_weights)
    {
        // w.wcls = std::move(w.token_embedding_table);
        q_matmul(s.logits.get(), s.xq.get(), w.q_tokens.get(), config.dim, config.vocab_size);
    }
    else
    {
        q_matmul(s.logits.get(), s.xq.get(), w.wcls.get(), config.dim, config.vocab_size);
    }
    return s.logits.get();
}

void Tokenizer::build_tokenizer(const std::string &tokenizer_path, int size_for_vacab)
{
    vocab_size = size_for_vacab;
    vocab.resize(vocab_size);
    vocab_scores.resize(vocab_size);
    for (int i = 0; i < 256; i++)
    {
        byte_pieces[i * 2] = static_cast<unsigned char>(i);
        byte_pieces[i * 2 + 1] = '\0';
    }
    std::ifstream file(tokenizer_path, std::ios::binary);
    if (!file)
    {
        std::cerr << "Couldn't open file " << tokenizer_path << '\n';
        std::exit(EXIT_FAILURE);
    }
    file.read(reinterpret_cast<char *>(&max_token_length), sizeof(int));
    int len = 0;
    for (int i = 0; i < vocab_size; i++)
    {
        file.read(reinterpret_cast<char *>(&vocab_scores[i]), sizeof(float));
        file.read(reinterpret_cast<char *>(&len), sizeof(int));
        vocab[i] = std::make_unique<char[]>(len + 1);
        file.read(vocab[i].get(), len);
        vocab[i][len] = '\0';
    }
    file.close();
}

void Sampler::build_sampler(int vocab_size, float temperature, float topp, unsigned long long rng_seed)
{
    this->vocab_size = vocab_size;
    this->temperature = temperature;
    this->topp = topp;
    rng_state = rng_seed;
    // buffer only used with nucleus sampling; may not need but it's ~small
    probindex = std::make_unique<ProbIndex[]>(vocab_size);
}

int Sampler::sample_argmax(float *probabilities, int n)
{
    // return the index that has the highest probability
    int max_i = 0;
    float max_p = probabilities[0];
    for (int i = 1; i < n; i++)
    {
        if (probabilities[i] > max_p)
        {
            max_i = i;
            max_p = probabilities[i];
        }
    }
    return max_i;
}

int Sampler::sample_mult(float *probabilities, int n, float coin)
{
    // sample index from probabilities (they must sum to 1!)
    // coin is a random number in [0, 1), usually from random_f32()
    float cdf = 0.0f;
    for (int i = 0; i < n; i++)
    {
        cdf += probabilities[i];
        if (coin < cdf)
        {
            return i;
        }
    }
    return n - 1; // in case of rounding errors
}

int Sampler::sample_topp(float *probabilities, int n, float topp, std::unique_ptr<ProbIndex[]> &probindex, float coin)
{
    // top-p sampling (or "nucleus sampling") samples from the smallest set of
    // tokens that exceed probability topp. This way we never sample tokens that
    // have very low probabilities and are less likely to go "off the rails".
    // coin is a random number in [0, 1), usually from random_f32()

    int n0 = 0;
    // quicksort indices in descending order of probabilities
    // values smaller than (1 - topp) / (n - 1) cannot be part of the result
    // so for efficiency we crop these out as candidates before sorting
    const float cutoff = (1.0f - topp) / (n - 1);
    for (int i = 0; i < n; i++)
    {
        if (probabilities[i] >= cutoff)
        {
            probindex[n0].index = i;
            probindex[n0].prob = probabilities[i];
            n0++;
        }
    }
    std::sort(probindex.get(), probindex.get() + n0, compare_probindex);

    // truncate the list where cumulative probability exceeds topp
    float cumulative_prob = 0.0f;
    int last_idx = n0 - 1; // in case of rounding errors consider all elements
    for (int i = 0; i < n0; i++)
    {
        cumulative_prob += probindex[i].prob;
        if (cumulative_prob > topp)
        {
            last_idx = i;
            break; // we've exceeded topp by including last_idx
        }
    }

    // sample from the truncated list
    float r = coin * cumulative_prob;
    float cdf = 0.0f;
    for (int i = 0; i <= last_idx; i++)
    {
        cdf += probindex[i].prob;
        if (r < cdf)
        {
            return probindex[i].index;
        }
    }
    return probindex[last_idx].index; // in case of rounding errors
}

int Sampler::sample(float *logits)
{
    // sample the token given the logits and some hyperparameters
    int next;
    if (temperature == 0.0f)
    {
        // greedy argmax sampling: take the token with the highest probability
        next = sample_argmax(logits, vocab_size);
    }
    else
    {
        // apply the temperature to the logits
        for (int q = 0; q < vocab_size; q++)
        {
            logits[q] /= temperature;
        }
        // apply softmax to the logits to get the probabilities for next token
        softmax(logits, vocab_size);
        // flip a (float) coin (this is our source of entropy for sampling)
        float coin = random_f32(&rng_state);
        // we sample from this distribution to get the next token
        if (topp <= 0 || topp >= 1)
        {
            // simply sample from the predicted probability distribution
            next = sample_mult(logits, vocab_size, coin);
        }
        else
        {
            // top-p (nucleus) sampling, clamping the least likely tokens to zero
            next = sample_topp(logits, vocab_size, topp, probindex, coin);
        }
    }
    return next;
}

void Tokenizer::encode(const std::string &text, const int8_t &bos, const int8_t &eos, std::unique_ptr<int[]> &tokens, int &n_tokens)
{
    // encode the string text (input) into an upper-bound preallocated tokens[] array
    // bos != 0 means prepend the BOS token (=1), eos != 0 means append the EOS token (=2)
    if (text.empty())
    {
        std::cerr << "cannot encode NULL text" << '\n';
        std::exit(EXIT_FAILURE);
    }

    if (!sorted_vocab)
    {
        // lazily malloc and sort the vocabulary
        sorted_vocab = std::make_unique<TokenIndex[]>(vocab_size);
        for (int i = 0; i < vocab_size; i++)
        {
            sorted_vocab[i].str = std::string(vocab[i].get());
            sorted_vocab[i].id = i;
        }
        std::sort(sorted_vocab.get(), sorted_vocab.get() + vocab_size, compare_tokens);
    }

    // create a temporary buffer that will store merge candidates of always two consecutive tokens
    // *2 for concat, +1 for null terminator +2 for UTF8 (in case max_token_length is 1)
    std::string str_buffer;
    str_buffer.resize(max_token_length * 2 + 1 + 2);
    size_t str_len = 0;

    // start at 0 tokens
    n_tokens = 0;

    // add optional BOS (=1) token, if desired
    if (bos)
        tokens[(n_tokens)++] = 1;

    // add_dummy_prefix is true by default
    // so prepend a dummy prefix token to the input string, but only if text != ""
    // TODO: pretty sure this isn't correct in the general case but I don't have the
    // energy to read more of the sentencepiece code to figure out what it's doing
    if (text[0] != '\0')
    {
        int dummy_prefix = str_lookup(" ", sorted_vocab, vocab_size);
        tokens[(n_tokens)++] = dummy_prefix;
    }

    // Okay UTF-8 time. This will get messy. Here is the reference from Wikipedia:
    // Code point ↔ UTF-8 conversion
    // First code point	Last code point	Byte 1	Byte 2	Byte 3	Byte 4
    // U+0000	U+007F	    0xxxxxxx
    // U+0080	U+07FF	    110xxxxx	10xxxxxx
    // U+0800	U+FFFF	    1110xxxx	10xxxxxx	10xxxxxx
    // U+10000	U+10FFFF    11110xxx	10xxxxxx	10xxxxxx	10xxxxxx

    // process the raw (UTF-8) byte sequence of the input string
    for (const char *c = text.c_str(); *c != '\0'; c++)
    {
        // reset buffer if the current byte is ASCII or a leading byte
        // 0xC0 is 11000000, so (*c & 0xC0) keeps the first 2 bits and zeros the rest
        // 0x80 is 10000000
        // in UTF-8, all continuation bytes start with "10" in first two bits
        // so in English this is: "if this byte is not a continuation byte"
        if ((*c & 0xC0) != 0x80)
        {
            // this byte must be either a leading byte (11...) or an ASCII char (0x...)
            // => reset our location, as we're starting a new UTF-8 codepoint
            str_len = 0;
        }

        // append the current byte to the buffer
        str_buffer[str_len++] = *c; // ++ is post-increment, incremented after this line
        str_buffer[str_len] = '\0';

        // while the next character is a continuation byte, continue appending
        // but if there are too many of them, just stop to avoid overrunning str_buffer size.
        if ((*(c + 1) & 0xC0) == 0x80 && str_len < 4)
        {
            continue;
        }

        // ok c+1 is not a continuation byte, so we've read in a full codepoint
        int id = str_lookup(str_buffer, sorted_vocab, vocab_size);

        if (id != -1)
        {
            // we found this codepoint in vocab, add it as a token
            tokens[(n_tokens)++] = id;
        }
        else
        {
            // byte_fallback encoding: just encode each byte as a token
            // +3 is here because the first 3 vocab elements are <unk>, <s>, </s>
            // so the individual bytes only start at index 3
            for (decltype(str_len) i = 0; i < str_len; i++)
            {
                tokens[(n_tokens)++] = (unsigned char) str_buffer[i] + 3;
            }
        }
        str_len = 0; // protect against a sequence of stray UTF8 continuation bytes
    }

    // merge the best consecutive pair each iteration, according the scores in vocab_scores
    while (1)
    {
        float best_score = -1e10;
        int best_id = -1;
        int best_idx = -1;

        for (int i = 0; i < (n_tokens - 1); i++)
        {
            // check if we can merge the pair (tokens[i], tokens[i+1])
            // sprintf(str_buffer, "%s%s", t->vocab[tokens[i]], t->vocab[tokens[i+1]]);
            int id = str_lookup(str_buffer, sorted_vocab, vocab_size);
            if (id != -1 && vocab_scores[id] > best_score)
            {
                // this merge pair exists in vocab! record its score and position
                best_score = vocab_scores[id];
                best_id = id;
                best_idx = i;
            }
        }

        if (best_idx == -1)
        {
            break; // we couldn't find any more pairs to merge, so we're done
        }

        // merge the consecutive pair (best_idx, best_idx+1) into new token best_id
        tokens[best_idx] = best_id;
        // delete token at position best_idx+1, shift the entire sequence back 1
        for (int i = best_idx + 1; i < (n_tokens - 1); i++)
        {
            tokens[i] = tokens[i + 1];
        }
        (n_tokens)--; // token length decreased
    }

    // add optional EOS (=2) token, if desired
    if (eos)
        tokens[(n_tokens)++] = 2;
}

std::string Tokenizer::decode(int prev_token, int token)
{
    char *piece = vocab[token].get();
    // following BOS (1) token, sentencepiece decoder strips any leading whitespace (see PR #89)
    if (prev_token == 1 && piece[0] == ' ')
    {
        piece++;
    }
    // careful, some tokens designate raw bytes, and look like e.g. '<0x01>'
    // parse this and convert and return the actual byte
    unsigned char byte_val;
    if (sscanf(piece, "<0x%02hhX>", &byte_val) == 1)
    {
        piece = (char *) byte_pieces + byte_val * 2;
    }
    return std::string(piece);
}

template<typename T>
void generate(Transformer<T> &transformer, Tokenizer &tokenizer, Sampler &sampler, std::string &prompt, int steps)
{
    std::string empty_prompt(1, '\0');
    if (prompt.empty())
    {
        prompt = empty_prompt;
    }
    // encode the (string) prompt into tokens sequence
    int num_prompt_tokens = 0;
    std::unique_ptr<int[]> prompt_tokens = std::make_unique<int[]>(strlen(prompt.c_str()) + 3); // +3 for '\0', ?BOS, ?EOS
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!encode(tokenizer, prompt, 1, 0, prompt_tokens, &num_prompt_tokens);
    tokenizer.encode(prompt, 1, 0, prompt_tokens, num_prompt_tokens);
    if (num_prompt_tokens < 1)
    {
        std::cerr << "something is wrong, expected at least 1 prompt token" << '\n';
        std::exit(EXIT_FAILURE);
    }
    // start the main loop
    long start = 0;               // used to time our code, only initialized after first iteration
    int next;                     // will store the next token in the sequence
    int token = prompt_tokens[0]; // kick off with the first token in the prompt
    int pos = 0;                  // position in the sequence
    while (pos < steps)
    {
        // forward the transformer to get logits for the next token
        float *logits = transformer.forward(token, pos);

        // advance the state machine
        if (pos < num_prompt_tokens - 1)
        {
            // if we are still processing the input prompt, force the next prompt token
            next = prompt_tokens[pos + 1];
        }
        else
        {
            // otherwise sample the next token from the logits
            next = sampler.sample(logits);
        }
        pos++;

        // data-dependent terminating condition: the BOS (=1) token delimits sequences
        if (next == 1)
        {
            break;
        }

        // print the token as string, decode it with the Tokenizer object
        std::string piece = tokenizer.decode(token, next);
        safe_print(piece); // same as printf("%s", piece), but skips "unsafe" bytes
        fflush(stdout);
        token = next;

        // init the timer here because the first iteration can be slower
        if (start == 0)
        {
            start = time_in_ms();
        }
    }
    std::cout << "\n";

    // report achieved tok/s (pos-1 because the timer starts after first iteration)
    if (pos > 1)
    {
        long end = time_in_ms();
        std::cerr << "achieved tok/s: " << (pos - 1) / static_cast<double>(end - start) * 1000 << std::endl;
    }
}

template<typename T>
void chat(Transformer<T> &transformer, Tokenizer &tokenizer, Sampler &sampler, std::string &cli_user_prompt, std::string &cli_system_prompt, int steps)
{
    // buffers for reading the system prompt and user prompt from stdin
    // you'll notice they are somewhat haphazardly and unsafely set atm
    std::string system_prompt;
    std::string user_prompt;
    std::string rendered_prompt;
    int num_prompt_tokens = 0;
    std::unique_ptr<int[]> prompt_tokens = std::make_unique<int[]>(1152);
    int user_idx;

    // start the main loop
    int8_t user_turn = 1; // user starts
    int next;             // will store the next token in the sequence
    int token;            // stores the current token to feed into the transformer
    int pos = 0;          // position in the sequence
    while (pos < steps)
    {

        // when it is the user's turn to contribute tokens to the dialog...
        if (user_turn)
        {
            // get the (optional) system prompt at position 0
            if (pos == 0)
            {
                // at position 0, the user can also contribute a system prompt
                if (cli_system_prompt.empty())
                {
                    // system prompt was not passed in, attempt to get it from stdin
                    read_stdin("Enter system prompt (optional): ", system_prompt, sizeof(system_prompt));
                }
                else
                {
                    // system prompt was passed in, use it
                    system_prompt = cli_system_prompt;
                }
            }
            // get the user prompt
            if (pos == 0 && !cli_user_prompt.empty())
            {
                // user prompt for position 0 was passed in, use it
                user_prompt = cli_user_prompt;
            }
            else
            {
                // otherwise get user prompt from stdin
                read_stdin("User: ", user_prompt, sizeof(user_prompt));
            }
            // render user/system prompts into the Llama 2 Chat schema
            if (pos == 0 && !system_prompt.empty())
            {
                std::string system_template = "[INST] <<SYS>>\n" + system_prompt + "\n<</SYS>>\n\n" + user_prompt + " [/INST]";
                rendered_prompt = system_template;
            }
            else
            {
                std::string user_template = "[INST] " + user_prompt + " [/INST]";
                rendered_prompt = user_template;
            }
            // encode the rendered prompt into tokens
            tokenizer.encode(rendered_prompt, 1, 0, prompt_tokens, num_prompt_tokens);
            user_idx = 0; // reset the user index
            user_turn = 0;
            std::cout << "Assistant: ";
        }

        // determine the token to pass into the transformer next
        if (user_idx < num_prompt_tokens)
        {
            // if we are still processing the input prompt, force the next prompt token
            token = prompt_tokens[user_idx++];
        }
        else
        {
            // otherwise use the next token sampled from previous turn
            token = next;
        }
        // EOS (=2) token ends the Assistant turn
        if (token == 2)
        {
            user_turn = 1;
        }

        // forward the transformer to get logits for the next token
        float *logits = transformer.forward(token, pos);
        next = sampler.sample(logits);
        pos++;

        if (user_idx >= num_prompt_tokens && next != 2)
        {
            // the Assistant is responding, so print its output
            std::string piece = tokenizer.decode(token, next);
            safe_print(piece); // same as printf("%s", piece), but skips "unsafe" bytes
            fflush(stdout);
        }
        if (next == 2)
        {
            std::cout << "\n";
        }
    }
    std::cout << "\n";
}

int main(int argc, char *argv[])
{
    std::string checkpoint_path;
    std::string tokenizer_path = "/initrd/assets/tokenizer.bin";
    float temperature = 1.0f; // 0.0 = greedy deterministic. 1.0 = original. don't set higher
    float topp = 0.9f;        // top-p in nucleus sampling. 1.0 = off. 0.9 works well, but slower
    int steps = 256;          // number of steps to run for
    std::string prompt;
    unsigned long long rng_seed = 0; // seed rng with time by default
    std::string mode = "generate";
    std::string system_prompt;
    if (argc >= 2)
    {
        checkpoint_path = argv[1];
    }
    else
    {
        checkpoint_path = "/initrd/assets/stories15M.bin";
    }

    for (int i = 2; i < argc; i += 2)
    {
        // do some basic validation
        if (i + 1 >= argc)
        {
            error_usage();
        } // must have arg after flag
        if (argv[i][0] != '-')
        {
            error_usage();
        } // must start with dash
        if (strlen(argv[i]) != 2)
        {
            error_usage();
        } // must be -x (one dash, one letter)
        // read in the args
        if (argv[i][1] == 't')
        {
            temperature = atof(argv[i + 1]);
        }
        else if (argv[i][1] == 'p')
        {
            topp = atof(argv[i + 1]);
        }
        else if (argv[i][1] == 's')
        {
            rng_seed = atoi(argv[i + 1]);
        }
        else if (argv[i][1] == 'n')
        {
            steps = atoi(argv[i + 1]);
        }
        else if (argv[i][1] == 'i')
        {
            prompt = argv[i + 1];
        }
        else if (argv[i][1] == 'z')
        {
            tokenizer_path = argv[i + 1];
        }
        else if (argv[i][1] == 'm')
        {
            mode = argv[i + 1];
        }
        else if (argv[i][1] == 'y')
        {
            system_prompt = argv[i + 1];
        }
        else
        {
            error_usage();
        }
    }
    if (rng_seed <= 0)
        rng_seed = (unsigned int) time(NULL);
    if (temperature < 0.0)
        temperature = 0.0;
    if (topp < 0.0 || 1.0 < topp)
        topp = 0.9;
    if (steps < 0)
        steps = 0;
    if (is_quantized_model(checkpoint_path))
    {
        Transformer<QuantizedTensor> transformer;
        transformer.load_model(checkpoint_path);
        if (steps == 0 || steps > transformer.config.seq_len)
            steps = transformer.config.seq_len; // ovrerride to ~max length
        Tokenizer tokenizer;
        tokenizer.build_tokenizer(tokenizer_path, transformer.config.vocab_size);
        Sampler sampler;
        sampler.build_sampler(transformer.config.vocab_size, temperature, topp, rng_seed);

        // run!
        if (mode == "generate")
        {
            generate(transformer, tokenizer, sampler, prompt, steps);
        }
        else if (mode == "chat")
        {
            chat(transformer, tokenizer, sampler, prompt, system_prompt, steps);
        }
        else
        {
            std::cerr << "unknown mode: " << mode << "\n" << std::endl;
            error_usage();
        }
    }
    else
    {
        Transformer<float> transformer;
        transformer.load_model(checkpoint_path);
        if (steps == 0 || steps > transformer.config.seq_len)
            steps = transformer.config.seq_len; // ovrerride to ~max length
        Tokenizer tokenizer;
        tokenizer.build_tokenizer(tokenizer_path, transformer.config.vocab_size);
        Sampler sampler;
        sampler.build_sampler(transformer.config.vocab_size, temperature, topp, rng_seed);

        // run!
        if (mode == "generate")
        {
            generate(transformer, tokenizer, sampler, prompt, steps);
        }
        else if (mode == "chat")
        {
            chat(transformer, tokenizer, sampler, prompt, system_prompt, steps);
        }
        else
        {
            std::cerr << "unknown mode: " << mode << "\n" << std::endl;
            error_usage();
        }
    }

    return 0;
}
