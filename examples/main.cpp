#include <iostream>
#include <snot.hpp>

int main(void)
{
    snot::document doc("example4.snot");

    doc.save_file("test.snot", true);
    return 0;
}
