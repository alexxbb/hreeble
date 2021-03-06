import os
from waf_common import setup_houdini

HHOME = r'C:\Users\houal\OneDrive\Documents\houdini16.5'
DSO_HOME = os.path.join(HHOME, 'DSO')


def configure(conf):
	conf.env.MSVC_VERSIONS = ['msvc 14.0']
	conf.env.MSVC_TARGETS = ['x86_amd64']
	conf.setup_houdini()


def build(ctx):
	ctx.objects(source="src\Element.cpp", 
				target="objects",
				includes=['src', ctx.env.HFS_INC],
				defines=ctx.env.DEFINES)

	ctx.shlib(source="src\sop_hreeble.cpp",
				target='hreeble',
				includes=['src', ctx.env.HFS_INC],
				defines=ctx.env.DEFINES,
				use="objects")
	ctx.install_files(DSO_HOME, ['hreeble.dll'])
