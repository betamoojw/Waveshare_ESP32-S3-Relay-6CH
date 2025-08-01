# ESP32 project template

## Description

This is a project...

## Requirements

- [GitHub Actions](https://github.com/features/actions)
- [platformio](https://platformio.org/)
- [python](https://www.python.org/)

## GitHub Actions - Workflow

The release build happens in the `build & release` workflow: [build_release.yml](.github/workflows/build_release.yml).
It creates a release, after creation of a new git tag (named it like `v1.0.0`).

If you want to test the build on all merge w/o creating a tag then the `build` workflow is what you looking for: [build.yml](.github/workflows/build.yml)

## PlatformIO

[PlatformIO](https://platformio.org/) is a tool to create microcontroller apps for arduino platforms and compatibles (esp32). You can install the [Visual Studio Code extension](https://platformio.org/install/ide?install=vscode) in the [Visual Studio Code](https://code.visualstudio.com/) IDE.

## Python

There is a tiny python script needed to customize the firmware filenames within platformio, see documentation: https://docs.platformio.org/en/stable/scripting/examples/custom_program_name.html

The [extra_script.py](extra_script.py) script gets the platformio env (e.g. lolin32) and the git-tag for the firmware filename.
This is required to publish several firmware names in the github artifacts of a release.

## Get Started

<img src="doc/create-new-project-with-template.png" />

1. Login to github

2. Click on `Use this template` to create a new git repository
3. Implement your application in the [src/main.cpp](src/main.cpp)
4. Comment your new change in the [CHANGELOG.md](CHANGELOG.md) file
5. Push your changes


6. You can find your firmwares under `Releases` after the CI build finished

## CHANGELOG

You can write your changes in the [CHANGELOG.md](CHANGELOG.md) before you create a release. It will be shown under the release page.

## Example Release

see [Releases](https://github.com/mcuw/esp-ghbuild-template/releases) on the right sidemenu.

## Customize your project

You can reduce and adapt your required boards in the [platformio.ini](platformio.ini).

Update the [CHANGELOG.md](CHANGELOG.md) file before you are creating a new release. By creating a new git tag you trigger a new release which generate for you the firmwares.

## Supported boards

Buy on AliExpress (affiliate links) ...

- ESP32 S3
  - [ESP32-S3-Relay-6CH](https://www.waveshare.com/wiki/ESP32-S3-Relay-6CH)


## Disclaimer

Contribution and help - if you find an issue or wants to contribute then please do not hesitate to create a pull request or an issue.

We provide our build template as is, and we make no promises or guarantees about this code.
