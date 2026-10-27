#include "polaris_devices.hpp"
// The common retail Polaris ID must work without a previously tested subsystem.
static_assert(polaris::accepts_device(0x1002,0x67DF),"RX470/480/570/580 rejected");
static_assert(!polaris::tested_profile(0x67DF,0,0),"Untested retail profile marked tested");
static_assert(polaris::accepts_device(0x1002,0x67EF),"Polaris11 rejected");
static_assert(polaris::accepts_device(0x1002,0x6987),"Polaris12 rejected");
static_assert(polaris::accepts_device(0x1002,0x6FDF),"RX580 regression");
static_assert(polaris::accepts_device(0x1002,0x699F),"RX550 regression");
static_assert(!polaris::accepts_device(0x10DE,0x67DF),"Wrong vendor accepted");
static_assert(!polaris::accepts_device(0x1002,0x731F),"Navi accepted as Polaris");
static_assert(!polaris::accepts_device(0x1002,0xFFFF),"Unknown device accepted");
static_assert(polaris::tested_profile(0x699F,0x0B041002,0xC7),"RX550 profile lost");
static_assert(!polaris::tested_profile(0x699F,0x12341002,0xC7),"Different board marked tested");
int main(){return 0;}
