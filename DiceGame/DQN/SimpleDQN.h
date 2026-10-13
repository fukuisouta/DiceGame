#pragma once
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <fstream>
#include <string>

class SimpleDQN
{
public:
    SimpleDQN(int stateSize = 72, int hiddenSize = 64, int actionSize = 4, float learningRate = 0.01f)
        : m_stateSize(stateSize), m_hiddenSize(hiddenSize), m_actionSize(actionSize), m_lr(learningRate)
    {
        std::mt19937 gen(1337);
        std::normal_distribution<float> dist(0.0f, 0.1f);

        m_w1.resize(stateSize * hiddenSize);
        for (auto& w : m_w1)
            w = dist(gen);
        m_b1.assign(hiddenSize, 0.0f);

        m_w2.resize(hiddenSize * actionSize);
        for (auto& w : m_w2)
            w = dist(gen);
        m_b2.assign(actionSize, 0.0f);
    }

    // 順伝播（推論: Q値の予測）
    std::vector<float> Predict(const std::vector<float>& state) const
    {
        std::vector<float> hidden(m_hiddenSize, 0.0f);
        std::vector<float> qValues(m_actionSize, 0.0f);

        for (int h = 0; h < m_hiddenSize; ++h)
        {
            float sum = m_b1[h];
            for (int s = 0; s < m_stateSize; ++s)
            {
                sum += state[s] * m_w1[s * m_hiddenSize + h];
            }
            hidden[h] = std::max(0.0f, sum); // ReLU
        }

        for (int a = 0; a < m_actionSize; ++a)
        {
            float sum = m_b2[a];
            for (int h = 0; h < m_hiddenSize; ++h)
            {
                sum += hidden[h] * m_w2[h * m_actionSize + a];
            }
            qValues[a] = sum;
        }

        return qValues;
    }

    // 逆伝播（学習: 重み更新）
    void Train(const std::vector<float>& state, int action, float targetQ)
    {
        std::vector<float> hiddenPre(m_hiddenSize, 0.0f);
        std::vector<float> hidden(m_hiddenSize, 0.0f);
        std::vector<float> qValues(m_actionSize, 0.0f);

        for (int h = 0; h < m_hiddenSize; ++h)
        {
            float sum = m_b1[h];
            for (int s = 0; s < m_stateSize; ++s)
            {
                sum += state[s] * m_w1[s * m_hiddenSize + h];
            }
            hiddenPre[h] = sum;
            hidden[h] = std::max(0.0f, sum);
        }

        for (int a = 0; a < m_actionSize; ++a)
        {
            float sum = m_b2[a];
            for (int h = 0; h < m_hiddenSize; ++h)
            {
                sum += hidden[h] * m_w2[h * m_actionSize + a];
            }
            qValues[a] = sum;
        }

        std::vector<float> deltaOutput(m_actionSize, 0.0f);
        deltaOutput[action] = qValues[action] - targetQ;

        std::vector<float> deltaHidden(m_hiddenSize, 0.0f);
        for (int h = 0; h < m_hiddenSize; ++h)
        {
            float error = 0.0f;
            for (int a = 0; a < m_actionSize; ++a)
            {
                error += deltaOutput[a] * m_w2[h * m_actionSize + a];
            }
            deltaHidden[h] = (hiddenPre[h] > 0.0f) ? error : 0.0f;
        }

        for (int h = 0; h < m_hiddenSize; ++h)
        {
            for (int a = 0; a < m_actionSize; ++a)
            {
                m_w2[h * m_actionSize + a] -= m_lr * deltaOutput[a] * hidden[h];
            }
            m_b2[h] -= m_lr * deltaOutput[h];
        }

        for (int s = 0; s < m_stateSize; ++s)
        {
            for (int h = 0; h < m_hiddenSize; ++h)
            {
                m_w1[s * m_hiddenSize + h] -= m_lr * deltaHidden[h] * state[s];
            }
        }
        for (int h = 0; h < m_hiddenSize; ++h)
        {
            m_b1[h] -= m_lr * deltaHidden[h];
        }
    }

    // セーブ＆ロード
    bool Save(const std::string& filename) const
    {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open())
            return false;

        file.write(reinterpret_cast<const char*>(m_w1.data()), m_w1.size() * sizeof(float));
        file.write(reinterpret_cast<const char*>(m_b1.data()), m_b1.size() * sizeof(float));
        file.write(reinterpret_cast<const char*>(m_w2.data()), m_w2.size() * sizeof(float));
        file.write(reinterpret_cast<const char*>(m_b2.data()), m_b2.size() * sizeof(float));
        return true;
    }

    bool Load(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open())
            return false;

        file.read(reinterpret_cast<char*>(m_w1.data()), m_w1.size() * sizeof(float));
        file.read(reinterpret_cast<char*>(m_b1.data()), m_b1.size() * sizeof(float));
        file.read(reinterpret_cast<char*>(m_w2.data()), m_w2.size() * sizeof(float));
        file.read(reinterpret_cast<char*>(m_b2.data()), m_b2.size() * sizeof(float));
        return true;
    }

private:
    int m_stateSize;
    int m_hiddenSize;
    int m_actionSize;
    float m_lr;

    std::vector<float> m_w1;
    std::vector<float> m_b1;
    std::vector<float> m_w2;
    std::vector<float> m_b2;
};