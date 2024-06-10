** for generic code style info, please see .clang-format **
** TODO FIXME NOT FINISHED YET!**
# Namespaces
- All "vanilla" code must be located in `namespace pb`, wile any optional extensions - *somewhere* else.
- All external dependencies MUST BE located in the `./external` directory
  - Extenral directory also contains all shared custom stuff.
- All the game systems SHOULD be located inside `namespace pb::game` and MUST BE loacted in the `./game` foder!
- No exceptions allowed! THREAT EXCEPTION AS TERMINATION.
  - All code that throws exceptions (except for std::bad_alloc() and assert-like conditions (out of bounds, fatal errors, etc.)) SHOULD BE REWRITTEN
  - Exception : per-thread global catch. For a reasons...
- NO DEEP LEVEL INHERITANCE ALLOWED!
  - Max level of inheritance : 3.
- DON'T DO HEAVY AND ERROR-PRONE ACTIONS IN CONSTRUCTORS! (especially one that throws exceptions!)
  - Use init()/uninit()/open()/close() methods for that!