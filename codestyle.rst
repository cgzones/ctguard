Coding Style
============

* clang-format compliant

* include order (from https://google.github.io/styleguide/cppguide.html#Names_and_Order_of_Includes):

  In dir/foo.cc or dir/foo_test.cc, whose main purpose is to implement or test the stuff in dir2/foo2.h, order your includes as follows:

  - dir2/foo2.h.
  - A blank line
  - C system headers (more precisely: headers in angle brackets with the .h extension), e.g. <unistd.h>, <stdlib.h>.
  - A blank line
  - C++ standard library headers (without file extension), e.g. <algorithm>, <cstddef>.
  - A blank line
  - Other libraries' .h files.
  - Your project's .h files.
