#define TORCH_ASSERT_ONLY_METHOD_OPERATORS
#include <ATen/core/Tensor.h>
#include <ATen/TensorIterator.h>
#include <ATen/native/Activation.h>

#ifndef AT_PER_OPERATOR_HEADERS
#include <ATen/Functions.h>
#include <ATen/NativeFunctions.h>
#else
#include <ATen/ops/empty.h>
#include <ATen/ops/glu_backward_native.h>
#include <ATen/ops/glu_native.h>
#include <ATen/ops/sigmoid.h>
#endif

namespace at {

namespace meta {
TORCH_META_FUNC(glu) (
    const Tensor& self, int64_t dim
) {
  // this can't pass anyway because a 0-dimensional tensor has "size" 1, which
  // can't be evenly halved, but give a nicer error message here.
  TORCH_CHECK(self.dim() > 0, "glu does not support 0-dimensional tensors");
  auto wrap_dim = maybe_wrap_dim(dim, self.dim());
  const int64_t nIn = self.size(wrap_dim);
  TORCH_CHECK(nIn % 2 == 0, "Halving dimension must be even, but dimension ",
              wrap_dim, " is size ", nIn);

  // size output to half of input
  const int64_t selfSize = nIn / 2;
  Tensor firstHalf = self.narrow(wrap_dim, 0, selfSize);
  Tensor secondHalf = self.narrow(wrap_dim, selfSize, selfSize);
  build_borrowing_binary_op(maybe_get_output(), firstHalf, secondHalf);
}
} // namespace meta

namespace native {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
DEFINE_DISPATCH(glu_stub);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
DEFINE_DISPATCH(glu_backward_stub);

TORCH_IMPL_FUNC(glu_out) (const Tensor& self, int64_t dim, const Tensor& out) {
  glu_stub(device_type(), *this);
}

Tensor& glu_backward_cpu_out(const Tensor& grad_output, const Tensor& input,
                             int64_t dim, Tensor& grad_input) {
  TORCH_CHECK(input.dim() > 0, "glu does not support 0-dimensional tensors");
  auto wrap_dim = maybe_wrap_dim(dim, input.dim());
  const int64_t nIn = input.size(wrap_dim);
  TORCH_CHECK(nIn % 2 == 0, "Halving dimension must be even, but dimension ",
              wrap_dim, " is size ", nIn);

  grad_input.resize_as_(input);
  const int64_t inputSize = nIn / 2;
  // half tensor
  Tensor firstHalf = input.narrow(wrap_dim, 0, inputSize);
  Tensor secondHalf = input.narrow(wrap_dim, inputSize, inputSize);
  Tensor gradInputfirstHalf = grad_input.narrow(wrap_dim, 0, inputSize);
  Tensor gradInputsecondHalf = grad_input.narrow(wrap_dim, inputSize, inputSize);

  at::sigmoid_out(gradInputfirstHalf, secondHalf);
  // for second gradinput half, can get a better performance by fusion
  auto iter = at::TensorIteratorConfig()
    .add_output(gradInputsecondHalf)
    .add_input(gradInputfirstHalf)
    .add_input(firstHalf)
    .add_input(grad_output)
    .build();
  glu_backward_stub(iter.device_type(), iter);
  gradInputfirstHalf.mul_(grad_output);
  return grad_input;
}

Tensor glu_backward_cpu(const Tensor& grad_output, const Tensor& input, int64_t dim) {
  auto grad_input = at::empty({0}, input.options());
  return glu_backward_cpu_out(grad_output, input, dim, grad_input);
}

} // at::native
} // at
