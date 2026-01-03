# Core idea #
---
A simple storage-engine esque program that can read/write data in a new format [that I'll have a defined FS like thing for]

store [A FS like thing that writes stuff on to disk based on the config that the user has for the storage thing]
#### FLAGS/OPTIONS ####
- `--config config_file` [If not passed uses default]
- `--compression on|off` [Thinking of calling LZ4 lib for compressing stuff|maybe will switch to my own version later]
- `--disk disk_name`[default -> store.disk]
- `--disk_size bytes|VALUE[KB|MB|GB]` => [To be used while init, creates a disk file that's sort of the disk for reading|writing]
##### USAGE/COMMANDS #####
- `store init --config store.toml` [Creates superblock based on the toml file, the flags can be just put in the toml file instead of writing everytime] {A daemon will run in BG, not sure}
- `store write f_name < data` [Writes to the disk based on the store.toml] [Does not append]
- `store append f_name < data` [Writes data starting on from f_name's EOF, if file is not already present raises F_NO_EXIST ERR]
- `store read f_name` [Will read file ...]
- `store sync` [Not sure what for, but thinking that user will be able to pass remote locations in the .toml file onto which the data will be synced]
