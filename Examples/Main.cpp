#include "Builders/WindowBuilder.hpp"

int main(int argc, const char **argv)
{
    auto freyaWindow = fra::WindowBuilder()
                           .SetTitle("Space")
                           .SetWidth(1024)
                           .SetHeight(600)
                           .SetVSync(true)
                           .Build();

    freyaWindow->Run();

    return 0;
}