#ifndef RUN_H
#define RUN_H
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#if defined _WIN32
#include "win.h"
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

extern int GS;

typedef struct
{
    std::unique_ptr<int8_t[]> q; // quantized values
    std::unique_ptr<float[]> s;  // scaling factors
} QuantizedTensor;

class Config
{
  public:
    int dim;        // transformer dimension
    int hidden_dim; // for ffn layers
    int n_layers;   // number of layers
    int n_heads;    // number of query heads
    int n_kv_heads; // number of key/value heads (can be < query heads because of multiquery)
    int vocab_size; // vocabulary size, usually 256 (byte-level)
    int seq_len;    // max sequence length
};

template<typename T>
class TransformerWeights
{
  public:
    // token embedding table
    std::unique_ptr<float[]> token_embedding_table; // (vocab_size, dim)
    // final rmsnorm
    std::unique_ptr<float[]> rms_final_weight; // (dim,)
    // (optional) classifier weights for the logits, on the last layer
    // weights for rmsnorms
    std::unique_ptr<float[]> rms_att_weight; // (layer, dim) rmsnorm weights
    std::unique_ptr<float[]> rms_ffn_weight; // (layer, dim)
    // weights for matmuls. note dim == n_heads * head_size
    std::unique_ptr<T[]> wq; // (layer, dim, n_heads * head_size)
    std::unique_ptr<T[]> wk; // (layer, dim, n_kv_heads * head_size)
    std::unique_ptr<T[]> wv; // (layer, dim, n_kv_heads * head_size)
    std::unique_ptr<T[]> wo; // (layer, n_heads * head_size, dim)
    // weights for ffn
    std::unique_ptr<T[]> w1; // (layer, hidden_dim, dim)
    std::unique_ptr<T[]> w2; // (layer, dim, hidden_dim)
    std::unique_ptr<T[]> w3; // (layer, hidden_dim, dim)
    std::unique_ptr<T[]> wcls;
    // tensor2d freq_cis_real;  // [seq_len, (dim/n_heads)/2]
    // tensor2d freq_cis_imag;  // [seq_len, (dim/n_heads)/2]
    std::unique_ptr<T[]> q_tokens; // (vocab_size, dim)
};

template<typename T>
class RunState
{
  public:
    // current wave of activations
    std::unique_ptr<float[]> x;      // activation at current time stamp (dim,)
    std::unique_ptr<float[]> xb;     // same, but inside a residual branch (dim,)
    std::unique_ptr<float[]> xb2;    // an additional buffer just for convenience (dim,)
    std::unique_ptr<float[]> hb;     // buffer for hidden dimension in the ffn (hidden_dim,)
    std::unique_ptr<float[]> hb2;    // buffer for hidden dimension in the ffn (hidden_dim,)
    std::unique_ptr<float[]> q;      // query (dim,)
    std::unique_ptr<float[]> k;      // key (dim,)
    std::unique_ptr<float[]> v;      // value (dim,)
    std::unique_ptr<float[]> att;    // buffer for scores/attention values (n_heads, seq_len)
    std::unique_ptr<float[]> logits; // output logits
    // kv cache
    std::unique_ptr<float[]> key_cache;   // (layer, seq_len, dim)
    std::unique_ptr<float[]> value_cache; // (layer, seq_len, dim)
};

template<>
class RunState<float>
{
  public:
    // current wave of activations
    std::unique_ptr<float[]> x;      // activation at current time stamp (dim,)
    std::unique_ptr<float[]> xb;     // same, but inside a residual branch (dim,)
    std::unique_ptr<float[]> xb2;    // an additional buffer just for convenience (dim,)
    std::unique_ptr<float[]> hb;     // buffer for hidden dimension in the ffn (hidden_dim,)
    std::unique_ptr<float[]> hb2;    // buffer for hidden dimension in the ffn (hidden_dim,)
    std::unique_ptr<float[]> q;      // query (dim,)
    std::unique_ptr<float[]> k;      // key (dim,)
    std::unique_ptr<float[]> v;      // value (dim,)
    std::unique_ptr<float[]> att;    // buffer for scores/attention values (n_heads, seq_len)
    std::unique_ptr<float[]> logits; // output logits
    // kv cache
    std::unique_ptr<float[]> key_cache;   // (layer, seq_len, dim)
    std::unique_ptr<float[]> value_cache; // (layer, seq_len, dim)
};

template<>
class RunState<QuantizedTensor>
{
  public:
    // current wave of activations
    std::unique_ptr<float[]> x;      // activation at current time stamp (dim,)
    std::unique_ptr<float[]> xb;     // same, but inside a residual branch (dim,)
    std::unique_ptr<float[]> xb2;    // an additional buffer just for convenience (dim,)
    std::unique_ptr<float[]> hb;     // buffer for hidden dimension in the ffn (hidden_dim,)
    std::unique_ptr<float[]> hb2;    // buffer for hidden dimension in the ffn (hidden_dim,)
    std::unique_ptr<float[]> q;      // query (dim,)
    std::unique_ptr<float[]> k;      // key (dim,)
    std::unique_ptr<float[]> v;      // value (dim,)
    std::unique_ptr<float[]> att;    // buffer for scores/attention values (n_heads, seq_len)
    std::unique_ptr<float[]> logits; // output logits
    // kv cache
    std::unique_ptr<float[]> key_cache;   // (layer, seq_len, dim)
    std::unique_ptr<float[]> value_cache; // (layer, seq_len, dim)

    std::unique_ptr<QuantizedTensor[]> xq; // quantized x (dim,)
    std::unique_ptr<QuantizedTensor[]> hq; // quantized hb (hidden_dim,)
};

typedef struct
{
    std::string str;
    int id;
} TokenIndex;

inline bool compare_tokens(const TokenIndex &a, const TokenIndex &b)
{
    return a.str < b.str;
}

inline int str_lookup(const std::string &str, const std::unique_ptr<TokenIndex[]> &sorted_vocab, int vocab_size)
{
    // efficiently find the perfect match for str in vocab, return its index or -1 if not found
    TokenIndex tok = { .str = str }; // acts as the key to search for

    auto it = std::lower_bound(sorted_vocab.get(), sorted_vocab.get() + vocab_size, tok, compare_tokens);

    // If we didn't reach the end and the string matches
    if (it != (sorted_vocab.get() + vocab_size) && it->str == str)
    {
        return it->id;
    }

    return -1; // Not found
}

template<typename T>
class Transformer
{
  private:
    void malloc_weights();
    void malloc_run_state();

  public:
    Config config;
    TransformerWeights<T> w;
    RunState<T> s;
    int shared_weights = 1;
    void load_model(const std::string &checkpoint_path);
    float *forward(int token, int pos);
};

class Tokenizer
{
  public:
    std::vector<std::unique_ptr<char[]>> vocab;
    std::vector<float> vocab_scores;
    std::unique_ptr<TokenIndex[]> sorted_vocab;
    int vocab_size;
    unsigned int max_token_length;
    unsigned char byte_pieces[512]; // stores all single-byte strings
    void build_tokenizer(const std::string &tokenizer_path, int size_for_vacab);
    void encode(const std::string &text, const int8_t &bos, const int8_t &eos, std::unique_ptr<int[]> &tokens, int &n_tokens);
    std::string decode(int prev_token, int token);
};

// ----------------------------------------------------------------------------
// The Sampler, which takes logits and returns a sampled token
// sampling can be done in a few ways: greedy argmax, sampling, top-p sampling
typedef struct
{
    float prob;
    int index;
} ProbIndex; // struct used when sorting probabilities during top-p sampling

class Sampler
{
  private:
    int sample_argmax(float *probabilities, int n);
    int sample_mult(float *probabilities, int n, float coin);
    int sample_topp(float *probabilities, int n, float topp, std::unique_ptr<ProbIndex[]> &probindex, float coin);
    unsigned int random_u32(unsigned long long *state)
    {
        // xorshift rng: https://en.wikipedia.org/wiki/Xorshift#xorshift.2A
        *state ^= *state >> 12;
        *state ^= *state << 25;
        *state ^= *state >> 27;
        return (*state * 0x2545F4914F6CDD1Dull) >> 32;
    }
    float random_f32(unsigned long long *state)
    { // random float32 in [0,1)
        return (random_u32(state) >> 8) / 16777216.0f;
    }
    static bool compare_probindex(const ProbIndex &a, const ProbIndex &b)
    {
        return a.prob > b.prob;
    }

  public:
    int vocab_size;
    std::unique_ptr<ProbIndex[]> probindex; // buffer used in top-p sampling
    float temperature;
    float topp;
    unsigned long long rng_state;
    void build_sampler(int vocab_size, float temperature, float topp, unsigned long long rng_seed);
    int sample(float *logits);
};

inline bool is_quantized_model(const std::string &checkpoint_path)
{
    std::ifstream file(checkpoint_path, std::ios::binary);
    if (!file)
    {
        std::cerr << "Couldn't open file " << checkpoint_path << '\n';
        std::exit(EXIT_FAILURE);
    }
    uint32_t magic_number;
    int version;

    file.read(reinterpret_cast<char *>(&magic_number), sizeof(uint32_t));
    file.read(reinterpret_cast<char *>(&version), sizeof(int));

    file.close();
    if (magic_number != 0x616b3432 || version != 2)
    {
        return false;
    }
    return true;
}

inline void safe_print(const std::string &piece)
{
    if (piece.empty())
    {
        return;
    }
    if (piece.size() == 1)
    {
        unsigned char byte_val = piece[0];
        if (!(isprint(byte_val) || isspace(byte_val)))
        {
            return; // bad byte, don't print it
        }
    }
    std::cout << piece;
}

inline long time_in_ms()
{
    // return time in milliseconds, for benchmarking the model speed
    auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

inline void softmax(float *x, int size)
{
    // find max value (for numerical stability)
    float max_val = x[0];
    for (int i = 1; i < size; i++)
    {
        if (x[i] > max_val)
        {
            max_val = x[i];
        }
    }
    // exp and sum
    float sum = 0.0f;
    for (int i = 0; i < size; i++)
    {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    // normalize
    for (int i = 0; i < size; i++)
    {
        x[i] /= sum;
    }
}

inline void rmsnorm(float *o, float *x, float *weight, int size)
{
    // calculate sum of squares
    float ss = 0.0f;
    for (int j = 0; j < size; j++)
    {
        ss += x[j] * x[j];
    }
    ss /= size;
    ss += 1e-5f;
    ss = 1.0f / sqrtf(ss);
    // normalize and scale
    for (int j = 0; j < size; j++)
    {
        o[j] = weight[j] * (ss * x[j]);
    }
}

inline void matmul(float *xout, float *x, float *w, int n, int d)
{
    // W (d,n) @ x (n,) -> xout (d,)
    // by far the most amount of time is spent inside this little function
    int i;
    for (i = 0; i < d; i++)
    {
        float val = 0.0f;
        for (int j = 0; j < n; j++)
        {
            val += w[i * n + j] * x[j];
        }
        xout[i] = val;
    }
}

inline void q_matmul(float *xout, QuantizedTensor *x, QuantizedTensor *w, int n, int d)
{
    // W (d,n) @ x (n,) -> xout (d,)
    // by far the most amount of time is spent inside this little function
    // inputs to this function are both quantized

    int i;
    for (i = 0; i < d; i++)
    {

        float val = 0.0f;
        int32_t ival = 0;
        int in = i * n;

        // do the matmul in groups of GS
        int j;
        for (j = 0; j <= n - GS; j += GS)
        {
            for (int k = 0; k < GS; k++)
            {
                ival += ((int32_t) x->q[j + k]) * ((int32_t) w->q[in + j + k]);
            }
            val += ((float) ival) * w->s[(in + j) / GS] * x->s[j / GS];
            ival = 0;
        }

        xout[i] = val;
    }
}

inline void read_stdin(const std::string &guide, std::string &buffer, size_t max_len)
{
    std::cout << guide;
    std::getline(std::cin, buffer);
    if (buffer.length() > max_len)
    {
        buffer.resize(max_len);
    }
}

inline void dequantize(QuantizedTensor *qx, float *x, int n)
{
    for (int i = 0; i < n; i++)
    {
        x[i] = qx->q[i] * qx->s[i / GS];
    }
}

inline void quantize(QuantizedTensor *qx, float *x, int n)
{
    int num_groups = n / GS;
    float Q_MAX = 127.0f;

    for (int group = 0; group < num_groups; group++)
    {
        // find the max absolute value in the current group
        float wmax = 0.0;
        for (int i = 0; i < GS; i++)
        {
            float val = fabs(x[group * GS + i]);
            if (val > wmax)
            {
                wmax = val;
            }
        }

        // calculate and write the scaling factor
        float scale = wmax / Q_MAX;
        qx->s[group] = scale;

        // calculate and write the quantized values
        for (int i = 0; i < GS; i++)
        {
            float quant_value = x[group * GS + i] / scale;  // scale
            int8_t quantized = (int8_t) round(quant_value); // round and clamp
            qx->q[group * GS + i] = quantized;
        }
    }
}

inline void init_quantized_tensors(std::ifstream &file, QuantizedTensor *w, int n_layers, int each_layer)
{

    for (int i = 0; i < n_layers; i++)
    {
        w[i].q = std::make_unique<int8_t[]>(each_layer);
        w[i].s = std::make_unique<float[]>(each_layer / GS);
        file.read(reinterpret_cast<char *>(w[i].q.get()), each_layer * sizeof(int8_t));
        file.read(reinterpret_cast<char *>(w[i].s.get()), each_layer / GS * sizeof(float));
    }
}

#endif
