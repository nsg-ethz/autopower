# Compiling client

This was tested on Debian 12.5 and should also work on Raspberry Pi OS from 15.03.2024. It is recommended to install this on a clean virtual machine since the self compiled GRPC version may pollute a local install of some libraries.

## Dependencies
* Install GRPC v1.63.0 or later like in the [official documentation](https://grpc.io/docs/languages/cpp/quickstart/) from source. This may take some time. If you are compiling on a Raspberry Pi, a Raspberry Pi 4 with at least 4 GB RAM is recommended.
* Install libjsoncpp-dev and libpqxx-dev: `apt install libjsoncpp-dev libpqxx-dev`
## Compile
**Hint:** If you changed the .proto file, you may need to run `./genCppProto.sh` before compiling.
To compile the client run:
```
mkdir -p cmake/build
cd cmake/build
cmake ../..
make -j 3
```
The binary `client` can be found in the cmake/build folder. This should be done on a Raspberry Pi if updates to the code were made. Otherwise, seting up cross compillation for arm64 should also be possible but is untested.

**Note:** To run the mmclient locally, you also need to [install PostgreSQL](https://wiki.debian.org/PostgreSql) on your machine. For the setup see POSTGRES-SETUP.md. You may also want to look at deploy.sh

## Pinpoint
To run the software, you also need to compile pinpoint. See [the README file from the official pinpoint repo](https://github.com/osmhpi/pinpoint/). You will need a version of Pinpoint which supports the `-n` flag to skip a workload and redirecting measurements to stdout. Currently this means you need to compile the code from this fork at `https://github.com/nsg-ethz/pinpoint/tree/ci/addARMBuild`.
You can clone and switch the branch as follows:

```shell
git clone https://github.com/nsg-ethz/pinpoint
git checkout ci/addARMBuild
```
then follow the same commands as in the official repo.
