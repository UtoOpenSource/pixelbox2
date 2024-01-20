** for generic code style info, please see .clang-format **
** TODO FIXME NOT FINISHED YET!**
# Namespaces
- All "vanilla" code must be located in `namespace pb`, wile any optional extensions - *somewhere* else (likely in `namespace extra`).
- All external dependencies MUST BE located in the `./external` directory, but ONLY IF THEY USED IN A DIRECT WAY AND THEY WAS NOT MODIFIED.
	- Exception for now : backend directory... yep
- All the  code, that creates an infrastructure for the game SHOULD be located inside `namespace pb::engine` and MUST BE located in the `./engine` floder!.
- All the game systems SHOULD be located inside `namespace pb::game` and MUST BE loacted in the `./game` foder!
- 