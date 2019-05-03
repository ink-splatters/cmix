#include "lstm-layer.h"

#include "sigmoid.h"

#include <math.h>
#include <algorithm>
#include <numeric>

namespace {

void Adam(std::valarray<float>* g, std::valarray<float>* m,
    std::valarray<float>* v, std::valarray<float>* w, float learning_rate,
    float t) {
  float beta1 = 0.025, beta2 = 0.9999, alpha = learning_rate * 0.067 /
      sqrt(5e-5 * t + 1), eps = 1e-6;
  (*m) *= beta1;
  (*m) += (1 - beta1) * (*g);
  (*v) *= beta2;
  (*v) += (1 - beta2) * (*g) * (*g);
  (*w) -= alpha * (((*m) / (float)(1 - pow(beta1, t))) /
      (sqrt((*v) / (float)(1 - pow(beta2, t)) + eps)));
}

}

LstmLayer::LstmLayer(unsigned int input_size, unsigned int auxiliary_input_size,
    unsigned int output_size, unsigned int num_cells, int horizon,
    float gradient_clip, float learning_rate) :
    forget_gate_(input_size, num_cells, horizon),
    input_node_(input_size, num_cells, horizon),
    output_gate_(input_size, num_cells, horizon),
    state_(num_cells), state_error_(num_cells), stored_error_(num_cells),
    tanh_state_(std::valarray<float>(num_cells), horizon),
    input_gate_state_(std::valarray<float>(num_cells), horizon),
    last_state_(std::valarray<float>(num_cells), horizon),
    gradient_clip_(gradient_clip), learning_rate_(learning_rate),
    num_cells_(num_cells), epoch_(0), horizon_(horizon),
    input_size_(auxiliary_input_size), output_size_(output_size) {
  float low = -0.2;
  float range = 0.4;
  for (unsigned int i = 0; i < num_cells_; ++i) {
    for (unsigned int j = 0; j < forget_gate_.weights_[i].size(); ++j) {
      forget_gate_.weights_[i][j] = low + Rand() * range;
      input_node_.weights_[i][j] = low + Rand() * range;
      output_gate_.weights_[i][j] = low + Rand() * range;
    }
    forget_gate_.weights_[i][forget_gate_.weights_[i].size() - 1] = 1;
  }
}

void LstmLayer::ForwardPass(const std::valarray<float>& input, int input_symbol,
    std::valarray<float>* hidden, int hidden_start) {
  last_state_[epoch_] = state_;
  for (unsigned int i = 0; i < num_cells_; ++i) {
    forget_gate_.state_[epoch_][i] = Sigmoid::Logistic(std::inner_product(
        &input[0], &input[input.size()],
        &forget_gate_.weights_[i][output_size_],
        forget_gate_.weights_[i][input_symbol]));
    input_node_.state_[epoch_][i] = tanh(std::inner_product(&input[0],
        &input[input.size()], &input_node_.weights_[i][output_size_],
        input_node_.weights_[i][input_symbol]));
    output_gate_.state_[epoch_][i] = Sigmoid::Logistic(std::inner_product(
        &input[0], &input[input.size()],
        &output_gate_.weights_[i][output_size_],
        output_gate_.weights_[i][input_symbol]));
  }
  input_gate_state_[epoch_] = 1.0f - forget_gate_.state_[epoch_];
  state_ *= forget_gate_.state_[epoch_];
  state_ += input_node_.state_[epoch_] * input_gate_state_[epoch_];
  tanh_state_[epoch_] = tanh(state_);
  std::slice slice = std::slice(hidden_start, num_cells_, 1);
  (*hidden)[slice] = output_gate_.state_[epoch_] * tanh_state_[epoch_];
  ++epoch_;
  if (epoch_ == horizon_) epoch_ = 0;
}

void LstmLayer::ClipGradients(std::valarray<float>* arr) {
  for (unsigned int i = 0; i < arr->size(); ++i) {
    if ((*arr)[i] < -gradient_clip_) (*arr)[i] = -gradient_clip_;
    else if ((*arr)[i] > gradient_clip_) (*arr)[i] = gradient_clip_;
  }
}

void LstmLayer::BackwardPass(const std::valarray<float>&input, int epoch,
    int layer, int input_symbol, std::valarray<float>* hidden_error) {
  if (epoch == (int)horizon_ - 1) {
    stored_error_ = *hidden_error;
    state_error_ = 0;
    for (unsigned int i = 0; i < num_cells_; ++i) {
      forget_gate_.update_[i] = 0;
      input_node_.update_[i] = 0;
      output_gate_.update_[i] = 0;
    }
  } else {
    stored_error_ += *hidden_error;
  }

  output_gate_.error_ = tanh_state_[epoch] * stored_error_ *
      output_gate_.state_[epoch] * (1.0f - output_gate_.state_[epoch]);
  state_error_ += stored_error_ * output_gate_.state_[epoch] * (1.0f -
      (tanh_state_[epoch] * tanh_state_[epoch]));
  input_node_.error_ = state_error_ * input_gate_state_[epoch] * (1.0f -
      (input_node_.state_[epoch] * input_node_.state_[epoch]));
  forget_gate_.error_ = (last_state_[epoch] - input_node_.state_[epoch]) *
      state_error_ * forget_gate_.state_[epoch] * input_gate_state_[epoch];

  *hidden_error = 0;
  if (layer > 0) {
    int offset = output_size_ + num_cells_ + input_size_;
    for (unsigned int i = 0; i < num_cells_; ++i) {
      for (unsigned int j = offset; j < offset + num_cells_; ++j) {
        (*hidden_error)[j-offset] += input_node_.weights_[i][j] *
            input_node_.error_[i];
        (*hidden_error)[j-offset] += forget_gate_.weights_[i][j] *
            forget_gate_.error_[i];
        (*hidden_error)[j-offset] += output_gate_.weights_[i][j] *
            output_gate_.error_[i];
      }
    }
  }

  if (epoch > 0) {
    state_error_ *= forget_gate_.state_[epoch];
    stored_error_ = 0;
    int offset = output_size_ + input_size_;
    for (unsigned int i = 0; i < num_cells_; ++i) {
      for (unsigned int j = offset; j < offset + num_cells_; ++j) {
        stored_error_[j-offset] += input_node_.weights_[i][j] *
            input_node_.error_[i];
        stored_error_[j-offset] += forget_gate_.weights_[i][j] *
            forget_gate_.error_[i];
        stored_error_[j-offset] += output_gate_.weights_[i][j] *
            output_gate_.error_[i];
      }
    }
  }

  ClipGradients(&state_error_);
  ClipGradients(&stored_error_);
  ClipGradients(hidden_error);

  std::slice slice = std::slice(output_size_, input.size(), 1);
  for (unsigned int i = 0; i < num_cells_; ++i) {
    forget_gate_.update_[i][slice] += forget_gate_.error_[i] * input;
    input_node_.update_[i][slice] += input_node_.error_[i] * input;
    output_gate_.update_[i][slice] += output_gate_.error_[i] * input;
    forget_gate_.update_[i][input_symbol] += forget_gate_.error_[i];
    input_node_.update_[i][input_symbol] += input_node_.error_[i];
    output_gate_.update_[i][input_symbol] += output_gate_.error_[i];
  }
  if (epoch == 0) {
    ++update_steps_;
    for (unsigned int i = 0; i < num_cells_; ++i) {
      Adam(&forget_gate_.update_[i], &forget_gate_.m_[i], &forget_gate_.v_[i],
          &forget_gate_.weights_[i], learning_rate_, update_steps_);
      Adam(&input_node_.update_[i], &input_node_.m_[i], &input_node_.v_[i],
          &input_node_.weights_[i], learning_rate_, update_steps_);
      Adam(&output_gate_.update_[i], &output_gate_.m_[i], &output_gate_.v_[i],
          &output_gate_.weights_[i], learning_rate_, update_steps_);
    }
  }
}
