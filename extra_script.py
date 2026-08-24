import os
Import("env")


def exclude_psychic_middlewares(_environment, _node):
	return None


env.AddBuildMiddleware(exclude_psychic_middlewares, "*/PsychicMiddlewares.cpp")

# Access to global construction environment
build_tag = env['PIOENV']
version_tag = os.getenv("FIRMWARE_VERSION")

env.Replace(PROGNAME="firmware_%s_%s" % (build_tag, version_tag))