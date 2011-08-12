import os
try:
	Import('sconscript')
except:
	sconscript = False

if not sconscript:
	variant_dir = "obj"
	sconscript = True
	work_dir = os.getcwd()	
	res = SConscript('SConstruct', dirs=['.'], variant_dir=variant_dir, 
			duplicate=0, exports=['sconscript','work_dir'])
	Return('res')

Import('work_dir')
env = Environment(
		LIBS = "ws2_32.lib"
		)

outpath = os.path.join(work_dir,"bin","socks.exe")
res = env.Program(outpath,[Glob("*.cpp")])
Return('res')

