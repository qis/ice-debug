// https://github.com/Microsoft/GSL
//
// Copyright (c) 2015 Microsoft Corporation. All rights reserved.
//
// This code is licensed under the MIT License (MIT).
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#pragma once
#include <utility>

namespace ice {

template <typename Handler>
class scope_exit {
public:
  explicit scope_exit(Handler handler) noexcept : handler_(std::move(handler)) {
  }

  scope_exit(scope_exit&& other) noexcept :
    handler_(std::move(other.handler_)), invoke_(std::exchange(other.invoke_, false)) {
  }

  scope_exit(const scope_exit& other) = delete;
  scope_exit& operator=(const scope_exit& other) = delete;

  ~scope_exit() noexcept(noexcept(handler_())) {
    if (invoke_) {
      handler_();
    }
  }

private:
  Handler handler_;
  bool invoke_ = true;
};

template <typename Handler>
auto on_scope_exit(Handler&& handler) noexcept {
  return ice::scope_exit<Handler>{ std::forward<Handler>(handler) };
}

}  // namespace ice
