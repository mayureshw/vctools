using namespace std;
#include <vector>
#include "vcsim.h"

int main()
{

// Data is represented by type Datum with a template argument as follows:
// It is important to select the correct type and width and it must match the
// corresponding paramter in the Aa program.

//      Unsigned integers:
//      Datum<uint8_t>   for widths up to 8   bits
//      Datum<uint16_t>  for widths up to 16  bits
//      Datum<uint32_t>  for widths up to 32  bits
//      Datum<uint64_t>  for widths up to 64  bits

//      Wide integer:
//      Datum<WUINT>     for widths up to 128 bits
//      WUINT is internally defined as bitset<WIDEUINTSZ>
//      WIDEUINTSZ is set to 128 (is configurable when compiling vctools)

//      Floating points:
//      Datum<float>     for single precision floating point numbers
//      Datum<double>    for double precision floating point numbers


//  In addition to type, the exact width of each variable is to be declared by
//  a constructor argument as shown below.
    Datum<uint8_t> a(8), b(8);

//  Datum instances can be initialized using the = operator
    a = 10; b = 20;

    vector<DatumBase*> inpv = {&a, &b};

//  See vcsim.h for detailed documentation of the interface
    vcsim("vcsim01.vc", "addi", inpv);

    return 0;
}
