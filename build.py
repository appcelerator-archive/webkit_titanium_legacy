#!/usr/bin/env python
import os, sys, re
import subprocess
import shutil
 
def uname_matches(n): return bool(re.match(os.uname()[0], n))
platform = None
if not hasattr(os, 'uname') or uname_matches('CYGWIN'):
	platform = 'win32'
elif uname_matches('Darwin'):
	platform = 'osx'
elif uname_matches('Linux'):
	platform = 'linux'

def run(args):
	return subprocess.Popen(args, stdout=subprocess.PIPE).communicate()[0]
 
print "Build Webkit_Titanium (%s).." % platform
try:
	if platform is 'linux':
		os.system("./configure")
		os.system("make")
	elif platform is 'osx':
		# TODO: add logic for 10.4/10.5 build
		# Ported over from the webkit nightly build script, although some parts not included...
		# They probably should be, but I don't understand what they do yet, --BTH
		#build_flags = 'DEBUG_INFORMATION_FORMAT=dwarf-with-dsym STRIP_INSTALLED_PRODUCT=YES DEPLOYMENT_POSTPROCESSING=YES'
		#build_flags_10_4 = 'GCC_VERSION=4.0 SDKROOT=/Developer/SDKs/MacOSX10.4u.sdk MACOSX_DEPLOYMENT_TARGET=10.4 WEBKIT_SYSTEM_INTERFACE_LIBRARY=WebKitSystemInterfaceTiger MAC_OS_X_VERSION_MAJOR=1040 MAC_OS_X_VERSION_ACTUAL=1049'
		#build_flags_10_5 = 'SDKROOT=/Developer/SDKs/MacOSX10.5.sdk MACOSX_DEPLOYMENT_TARGET=10.5 WEBKIT_SYSTEM_INTERFACE_LIBRARY=WebKitSystemInterfaceLeopard'
		build_flags = ' ARCHS="i386 ppc" ONLY_ACTIVE_ARCH=NO'
		os.system("./WebKitTools/Scripts/build-webkit --release " + build_flags)
	elif platform is 'win32':
		cygwin_dir = 'C:\\cygwin'
		bash_exe = os.path.join(cygwin_dir, 'bin', 'bash.exe')
		cygpath_exe = os.path.join(cygwin_dir, 'bin', 'cygpath.exe')
		workspace_dir = os.environ["WORKSPACE"]
		workspace_cyg_dir = run([cygpath_exe, "-u", workspace_dir])
		run([bash_exe, '-l', '-c', "%s/WebKitTools/Scripts/build-webkit" % workspace_cyg_dir, "--release", "--cairo-win32"])
except OSError, e:
  print "Error building: " + str(e)
