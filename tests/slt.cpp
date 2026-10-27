#include "polaris_slt.hpp"
#include <cassert>
int main(){
    assert(polaris::sid_to_slt(0x1B539A1E159ULL,0x51AC70470CULL)=="M766Q0211312");
    assert(polaris::sid_to_slt(0x29238F17C4BULL,0x8E33104714ULL)=="M10YG0182020");
    assert(polaris::sid_to_slt(0x1B539A1E159ULL,0x451AC70470CULL)=="M766Q0211312");
    assert(polaris::sid_to_slt(1ULL|(31ULL<<32)|(255ULL<<37),255)=="A31255255");
    bool rejected=false;try{polaris::sid_to_slt(1ULL<<50,0);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    return 0;
}
