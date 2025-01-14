#!/usr/bin/env python
# encoding: utf-8
# Carlos Rafael Giani, 2006 (dv)
# Tamas Pal, 2007 (folti)
# Nicolas Mercier, 2009
# Matt Clarkson, 2012
# Christopher Bolte: Created a copy to easier adjustment to crytek specific changes

import os, sys, re, tempfile
import xml.etree.ElementTree as ET
from waflib import Utils, Task, Logs, Options, Errors
from waflib.Logs import debug, warn
from waflib.Tools import ccroot, c, cxx, ar
from waflib.Configure import conf
from waflib.TaskGen import after_method, feature, before_method

@feature('cprogram', 'cshlib', 'cxxprogram', 'cxxshlib')
@after_method('apply_link')
def apply_manifest(self):
        """
        Special linker for MSVC with support for embedding manifests into DLL's
        and executables compiled by Visual Studio 2005 or probably later. Without
        the manifest file, the binaries are unusable.
        See: http://msdn2.microsoft.com/en-us/library/ms235542(VS.80).aspx
        """
        if self.env.CC_NAME == 'msvc' and self.env.MSVC_MANIFEST and getattr(self, 'link_task', None):                
                out_node = self.link_task.outputs[0]
                man_node = out_node.parent.find_or_declare(out_node.name + '.manifest')
                self.link_task.outputs.append(man_node)
                self.link_task.do_manifest = True

def exec_mf(self):
        """
        Create the manifest file
        """        
        env = self.env
        mtool = env['MT']
        if not mtool:
                return 0

        self.do_manifest = False

        outfile = self.outputs[0].abspath()

        manifest = None
        for out_node in self.outputs:
                if out_node.name.endswith('.manifest'):
                        manifest = out_node.abspath()
                        break
        if manifest is None:
                # Should never get here.  If we do, it means the manifest file was
                # never added to the outputs list, thus we don't have a manifest file
                # to embed, so we just return.
                return 0

        # embedding mode. Different for EXE's and DLL's.
        # see: http://msdn2.microsoft.com/en-us/library/ms235591(VS.80).aspx
        mode = ''
        if 'cprogram' in self.generator.features or 'cxxprogram' in self.generator.features:
                mode = '1'
        elif 'cshlib' in self.generator.features or 'cxxshlib' in self.generator.features:
                mode = '2'

        debug('msvc: embedding manifest in mode %r' % mode)

        lst = []
        lst.append(env['MT'])
        lst.extend(Utils.to_list(env['MTFLAGS']))
        lst.extend(['-manifest', manifest])
        if hasattr(self.generator, 'additional_manifests'):
			lst.append( self.generator.path.abspath() + '/' + self.generator.additional_manifests )
        lst.append('-outputresource:%s;%s' % (outfile, mode))

        lst = [lst]        
        return self.exec_command(*lst)
				
def quote_response_command(self, flag):
        if flag.find(' ') > -1:
                for x in ('/LIBPATH:', '/IMPLIB:', '/OUT:', '/I'):
                        if flag.startswith(x):
                                flag = '%s"%s"' % (x, flag[len(x):])
                                break
                else:
                        flag = '"%s"' % flag
        return flag

def exec_response_command(self, cmd, **kw):
	# not public yet
	try:
		tmp = None
		if sys.platform.startswith('win') and isinstance(cmd, list) and len(' '.join(cmd)) >= 8192:
		        tmp_files_folder = self.generator.bld.get_bintemp_folder_node().make_node('TempFiles')
		        program = cmd[0] #unquoted program name, otherwise exec_command will fail
		        cmd = [self.quote_response_command(x) for x in cmd]
		        out_file = os.path.split(cmd[-1])
		        (fd, tmp) = tempfile.mkstemp(prefix=out_file[1], dir=tmp_files_folder.abspath())
		        os.write(fd, '\r\n'.join(i.replace('\\', '\\\\') for i in cmd[1:]).encode())
		        os.close(fd)
		        cmd = [program, '@' + tmp]
		# no return here, that's on purpose
		ret = self.generator.bld.exec_command(cmd, **kw)

	finally:
		if tmp:
			try:
				os.remove(tmp)
			except OSError:
				pass # anti-virus and indexers can keep the files open -_-
	return ret

########## stupid evil command modification: concatenate the tokens /Fx, /doc, and /x: with the next token
import waflib.Node
def exec_command_msvc(self, *k, **kw):
	"""
	Change the command-line execution for msvc programs.
	Instead of quoting all the paths and keep using the shell, we can just join the options msvc is interested in
	"""
		
	# 1) Join options that carry no space are joined e.g. /Fo FilePath -> /FoFilePath
	# 2) Join options that carry a ':' as last character : e.g. /OUT: FilePath -> /OUT:FilePath
	if isinstance(k[0], list):
		lst = []
		carry = ''
		join_with_next_list_item = ['/Fo', '/doc', '/Fi', '/Fa']	
		for a in k[0]:
			if a in  join_with_next_list_item or a[-1] == ':':
				carry = a
			else:
				lst.append(carry + a)
				carry = ''

		k = [lst]
		
	bld = self.generator.bld
	try:
		if not kw.get('cwd', None):
			kw['cwd'] = bld.cwd
	except AttributeError:
		bld.cwd = kw['cwd'] = bld.variant_dir

	ret = self.exec_response_command(k[0], **kw)
	
	if not ret and getattr(self, 'do_manifest', None):
		ret = self.exec_mf()
	return ret
		
def wrap_class(class_name):
        """
        Manifest file processing and @response file workaround for command-line length limits on Windows systems
        The indicated task class is replaced by a subclass to prevent conflicts in case the class is wrapped more than once
        """    
        cls = Task.classes.get(class_name, None)
        if not cls:
                return

        derived_class = type(class_name, (cls,), {})
        def exec_command(self, *k, **kw):
                if self.env['CC_NAME'] == 'msvc':
                        return self.exec_command_msvc(*k, **kw)
                else:
                        return super(derived_class, self).exec_command(*k, **kw)

        # Chain-up monkeypatch needed since exec_command() is in base class API
        derived_class.exec_command = exec_command

        # No chain-up behavior needed since the following methods aren't in
        # base class API
        derived_class.exec_response_command = exec_response_command
        derived_class.quote_response_command = quote_response_command
        derived_class.exec_command_msvc = exec_command_msvc
        derived_class.exec_mf = exec_mf

        return derived_class

for k in 'c cxx cprogram cxxprogram cshlib cxxshlib cstlib cxxstlib'.split():
	wrap_class(k)
	

@feature('cxxprogram', 'cxxshlib', 'cprogram', 'cshlib', 'cxx', 'c')
@after_method('apply_incpaths')
@after_method('process_pch_msvc')
def set_pdb_flags(self):
	if not 'msvc' in (self.env.CC_NAME, self.env.CXX_NAME):
		return	

	# Not having PDBs stops CL.exe working for precompiled header when we have VCCompiler set to true for IB...
	if not self.bld.is_option_true('generate_debug_info'):
		return

	# Compute PDB file path
	pdb_folder = self.path.get_bld()
	pdb_file = pdb_folder.make_node(str(self.idx) + '.pdb')
	pdb_cxxflag = '/Fd' + pdb_file.abspath() + ''

	# Make sure the PDB folder exists
	pdb_folder.mkdir()
	
	if getattr(self, 'link_task', None) and self._type != 'stlib':
		pdbnode = self.link_task.outputs[0].change_ext('.pdb')
		self.link_task.outputs.append(pdbnode)

	# Add CXX and C Flags
	for t in getattr(self, 'compiled_tasks', []):
		# Test for PCH being used for this CXX/C task
		uses_pch = False
		for key in [ 'CFLAGS', 'CXXFLAGS' ]:
			for value in t.env[key]:
				if '/Yu' in value:
					uses_pch = True

		# Re-use the PDB flag from the PCH if it's used
		pdb_unique_cxxflag = pdb_cxxflag if uses_pch else '/Fd' + t.outputs[0].change_ext('.pdb').abspath()
		t.env.append_value('CXXFLAGS', pdb_unique_cxxflag)
		t.env.append_value('CFLAGS', pdb_unique_cxxflag)

		# In case of PCH, we need to also apply /FS
		msvc_version = 11
		try:
			msvc_version = float(t.env['MSVC_VERSION'])
		except:
			pass
		if uses_pch and msvc_version >= 12:
			t.env.append_value('CXXFLAGS', '/FS')
			t.env.append_value('CFLAGS', '/FS')

	# Add PDB also to Precompiled header
	for t in self.tasks:
		if isinstance(t,pch_msvc):
			t.env.append_value('CXXFLAGS', pdb_cxxflag)
			t.env.append_value('CFLAGS', pdb_cxxflag)

#############################################################################
@feature('cxxprogram', 'cxxshlib', 'cprogram', 'cshlib', 'cxx', 'c')
@after_method('apply_link')
def apply_map_file(self):
	if not 'msvc' in (self.env.CC_NAME, self.env.CXX_NAME):
		return	

	# Dont create map files if not asked for
	if not self.bld.is_option_true('generate_map_file'):
		return
		
	if getattr(self, 'link_task', None) and self._type != 'stlib':
		map_file_node = self.link_task.outputs[0].change_ext('.map')
		self.link_task.outputs.append(map_file_node)
		self.env.append_value('LINKFLAGS', '/MAP:' + map_file_node.abspath())		

#############################################################################
#############################################################################
# Special handling to be able to create response files
# Based in msvc from WAF
def quote_response_command_orbis(self, flag):
	"""
	if flag.find(' ') > -1:
		for x in ('/LIBPATH:', '/IMPLIB:', '/OUT:', '/I'):
			if flag.startswith(x):
				flag = '%s"%s"' % (x, flag[len(x):])
				break
			else:
				flag = '"%s"' % flag
		return flag
	"""
	flag = '"%s"' % flag
	return flag
	
#############################################################################
def exec_response_command_orbis(self, cmd, **kw):
	# not public yet	
	try:
		tmp = None
		if sys.platform.startswith('win') and isinstance(cmd, list) and len(' '.join(cmd)) >= 8192:			
			program = cmd[0] #unquoted program name, otherwise exec_command will fail
			cmd = [self.quote_response_command_orbis(x) for x in cmd]
			(fd, tmp) = tempfile.mkstemp()
			os.write(fd, '\r\n'.join(i.replace('\\', '\\\\') for i in cmd[1:]).encode())
			os.close(fd)
			cmd = [program, '@' + tmp]
		# no return here, that's on purpose
		ret = self.generator.bld.exec_command(cmd, **kw)
	finally:
		if tmp:
			try:
				os.remove(tmp)
			except OSError:
				pass # anti-virus and indexers can keep the files open -_-
	return ret

#############################################################################
def exec_command_orbis_clang(self, *k, **kw):	
	"""
	if isinstance(k[0], list):
		lst = []
		carry = ''
		for a in k[0]:
			if a == '/Fo' or a == '/doc' or a[-1] == ':':
				carry = a
			else:
				lst.append(carry + a)
				carry = ''
			k = [lst]
			
		if self.env['PATH']:
			env = dict(self.env.env or os.environ)
			env.update(PATH = ';'.join(self.env['PATH']))
			kw['env'] = env
	"""
	bld = self.generator.bld
	try:
		if not kw.get('cwd', None):
			kw['cwd'] = bld.cwd
	except AttributeError:
		bld.cwd = kw['cwd'] = bld.variant_dir

	return self.exec_response_command_orbis(k[0], **kw)
	
#############################################################################
def wrap_class_orbis(class_name):
	"""
	@response file workaround for command-line length limits on Windows systems
	The indicated task class is replaced by a subclass to prevent conflicts in case the class is wrapped more than once
	"""
	cls = Task.classes.get(class_name, None)

	if not cls:
		return None

	derived_class = type(class_name, (cls,), {})

	def exec_command(self, *k, **kw):
		if self.env['CC_NAME'] == 'orbis-clang' or self.env['CC_NAME'] == 'gcc':
			return self.exec_command_orbis_clang(*k, **kw)
		else:
			return super(derived_class, self).exec_command(*k, **kw)

	# Chain-up monkeypatch needed since exec_command() is in base class API
	derived_class.exec_command = exec_command

	# No chain-up behavior needed since the following methods aren't in
	# base class API
	derived_class.exec_response_command_orbis = exec_response_command_orbis
	derived_class.quote_response_command_orbis = quote_response_command_orbis
	derived_class.exec_command_orbis_clang = exec_command_orbis_clang

	return derived_class

#############################################################################
## Wrap call exec commands		
for k in 'c cxx cprogram cxxprogram cshlib cxxshlib cstlib cxxstlib'.split():
	wrap_class_orbis(k)		
	
from waflib.TaskGen import feature, after	
from waflib.Tools import c_preproc
from waflib.Task import Task

@feature('cxx')
@after_method('apply_incpaths')
def process_pch_msvc(self):	
	if not 'msvc' in (self.env.CC_NAME, self.env.CXX_NAME):
		return
		
	if Utils.unversioned_sys_platform() != 'win32':
		return

	# Create Task to compile PCH
	if not getattr(self, 'pch', ''):
		return

	if not self.bld.is_option_true('use_precompiled_header'):
		return

	# Always assume only one PCH File
	pch_source = self.to_nodes(self.pch)[0]

	pch_header = pch_source.change_ext('.h')
	pch_header_name = os.path.split(pch_header.abspath())[1]

	# Generate PCH per target project idx 
	# Avoids the case where two project have the same PCH output path but compile the PCH with different compiler options i.e. defines, includes, ...
	pch_file = pch_source.change_ext('.%d.pch' % self.idx)
	pch_object = pch_source.change_ext('.%d.obj' % self.idx)
	# Create PCH Task
	self.pch_task = pch_task = self.create_task('pch_msvc', pch_source, [pch_object,pch_file])
	pch_task.env.append_value( 'PCH_NAME', pch_header_name )
	pch_task.env.append_value( 'PCH_FILE', '/Fp' + pch_file.abspath() )
	pch_task.env.append_value( 'PCH_OBJ', pch_object.abspath() )
	
	# Append PCH File to each compile task
	for t in getattr(self, 'compiled_tasks', []):	
		input_file = t.inputs[0].abspath()
		file_specific_settings = self.file_specifc_settings.get(input_file, None)
		if file_specific_settings and 'disable_pch' in file_specific_settings and file_specific_settings['disable_pch'] == True:
			continue # Don't append PCH to files for which we don't use them
			
		if getattr(t, 'disable_pch', False) == True:
			continue # Don't append PCH to files for which we don't use them
			
		if t.__class__.__name__ == 'cxx': #Is there a better way to ensure cpp only?
			pch_flag = '/Fp' + pch_file.abspath()
			pch_header = '/Yu' + pch_header_name
			t.env.append_value('CXXFLAGS', pch_header) 
			t.env.append_value('CXXFLAGS', pch_flag)

			# Append PCH to task input to ensure correct ordering
			t.dep_nodes.append(pch_object)

	
	if getattr(self, 'link_task', None):
		self.link_task.inputs.append(pch_object)
	
class pch_msvc(Task):
	run_str = '${CXX} ${PCH_CREATE_ST:PCH_NAME} ${CXXFLAGS} ${CPPPATH_ST:INCPATHS} ${DEFINES_ST:DEFINES} ${SRC} ${CXX_TGT_F}${PCH_OBJ} ${PCH_FILE}'	
	scan    = c_preproc.scan
	color   = 'BLUE'	
	
	def exec_command(self, *k, **kw):	
		return exec_command_msvc(self, *k, **kw)
	
	def exec_response_command(self, *k, **kw):	
		return exec_response_command(self, *k, **kw)	

	def quote_response_command(self, *k, **kw):	
		return quote_response_command(self, *k, **kw)

	def exec_mf(self, *k, **kw):	
		return exec_mf(self, *k, **kw)

from waflib.Tools import cxx,c
	
@feature('cxx')
@after_method('apply_incpaths')
def process_pch_orbis(self):

	if not 'orbis-clang' in (self.env.CC_NAME, self.env.CXX_NAME):
		return

	# Create Task to compile PCH
	if not getattr(self, 'pch', ''):
		return
	
	if not self.is_option_true('use_precompiled_header'):
		return
	
	# Always assume only one PCH File
	pch_source = self.to_nodes(self.pch)[0]

	pch_header = pch_source.change_ext('.h')	
	pch_file = pch_source.change_ext('.h.pch')
	
	# Create PCH Task
	self.create_task('pch_orbis', pch_source, pch_file)

	# Append PCH File to each compile task
	for t in getattr(self, 'compiled_tasks', []):	
		if t.__class__.__name__ == 'cxx': 
			# we need to get the absolute path to the pch.h.pch
			# which we then need to include as pch.h
			# Since WAF is so smart to generate the source path for the header
			# we do the name replacement outself :)
			pch_file_name = pch_file.abspath()
			pch_file_name = pch_file_name.replace('.h.pch', '.h')
			pch_flag = '-include' + pch_file_name

			t.env.append_value('CXXFLAGS', pch_flag) 	
			t.env.append_value('CFLAGS', pch_flag) 	
			
			# Append PCH to task input to ensure correct ordering
			t.dep_nodes.append(pch_file)

class pch_orbis(Task):
	run_str = '${CXX} -x c++-header ${CXXFLAGS} ${CPPPATH_ST:INCPATHS} ${DEFINES_ST:DEFINES} ${SRC} -o ${TGT}'
	scan    = c_preproc.scan
	color   = 'BLUE'
	
def strip_all_but_last_dependent_options(env, strip_list):
	# [CFLAGS] Delete all but last option in reverse as list size changes on each delete
	indices = [i for i, e in enumerate(env.CFLAGS,) if e in strip_list]	
	for idx in reversed(indices[:-1]):
		del env.CFLAGS[idx]
		
	# [CXXFLAGS] Delete all but last option in reverse as list size changes on each delete
	indices = [i for i, e in enumerate(env.CXXFLAGS,) if e in strip_list]	
	for idx in reversed(indices[:-1]):
		del env.CXXFLAGS[idx]
		
def strip_all_but_last_with_prefix(env, prefix):	
	# [CFLAGS] Delete all but last option in reverse as list size changes on each delete
	indices = [i for i, e in enumerate(env.CFLAGS,) if prefix in e]	
	for idx in reversed(indices[:-1]):
		del env.CFLAGS[idx]
		
	# [CXXFLAGS] Delete all but last option in reverse as list size changes on each delete
	indices = [i for i, e in enumerate(env.CXXFLAGS,) if prefix in e]	
	for idx in reversed(indices[:-1]):
		del env.CXXFLAGS[idx]
		
def replace_compiler_option(env, old, new):
	env.CFLAGS 	 = [w.replace(old, new) for w in env.CFLAGS]
	env.CXXFLAGS = [w.replace(old, new) for w in env.CXXFLAGS]
		
def verify_options_common(env):	
		
	#Optimization options
	strip_all_but_last_dependent_options(env, ["/O1", "/O2", "/Od", "/Ox"])	
	strip_all_but_last_with_prefix(env, "/Ob")
	strip_all_but_last_with_prefix(env, "/Oi")
	strip_all_but_last_with_prefix(env, "/Os")
	strip_all_but_last_with_prefix(env, "/Oy")
	strip_all_but_last_with_prefix(env, "/favor")

	#Code Generation               
	strip_all_but_last_with_prefix(env, "/arch")
	strip_all_but_last_with_prefix(env, "/clr")
	strip_all_but_last_with_prefix(env, "/EH")
	strip_all_but_last_with_prefix(env, "/fp")
	strip_all_but_last_dependent_options(env, ["/Gd", "/Gr", "/Gv", "/Gz"])
	strip_all_but_last_with_prefix(env, "/GL")
	strip_all_but_last_with_prefix(env, "/GR")
	strip_all_but_last_with_prefix(env, "/GS")
	strip_all_but_last_with_prefix(env, "/Gs")
	strip_all_but_last_with_prefix(env, "/Gw")
	strip_all_but_last_with_prefix(env, "/Gy")
	strip_all_but_last_with_prefix(env, "/Qpar")
	strip_all_but_last_with_prefix(env, "/Qvec")
	strip_all_but_last_with_prefix(env, "/RTC")	
	strip_all_but_last_with_prefix(env, "/volatile")

	#Language                               
	strip_all_but_last_with_prefix(env, "/vd")
	strip_all_but_last_dependent_options(env, ["/vmb", "/vmg"])
	strip_all_but_last_dependent_options(env, ["/vmm", "/vms", "/vmv"])
	strip_all_but_last_dependent_options(env, ["/Z7", "/Zi", "/ZI"])
	strip_all_but_last_dependent_options(env, ["/Za", "/Ze"])
	strip_all_but_last_with_prefix(env, "/Zp")
	strip_all_but_last_with_prefix(env, "/ZW")
	
	#Linking
	strip_all_but_last_dependent_options(env, ["/MD", "/MT", "/LD", "/MDd", "/MTd", "/LDd"])

	#Misc
	strip_all_but_last_with_prefix(env, "/errorReport")
	strip_all_but_last_with_prefix(env, "/sdl")
	strip_all_but_last_with_prefix(env, "/Zc:forScope")
	strip_all_but_last_with_prefix(env, "/Zc:wchar_t")
	strip_all_but_last_with_prefix(env, "/Zc:auto")
	strip_all_but_last_with_prefix(env, "/Zc:trigraphs")
	strip_all_but_last_with_prefix(env, "/Zc:rvalueCast")
	strip_all_but_last_with_prefix(env, "/Zc:strictStrings")
	strip_all_but_last_with_prefix(env, "/Zc:inline")
	strip_all_but_last_with_prefix(env, "/WX")
	strip_all_but_last_dependent_options(env, ["/W0", "/W1", "/W2", "/W3", "/W4"])
	
	# Backwards compatibility to msvc 11.0
	if env['MSVC_VERSION'] == '11.0':
		replace_compiler_option(env,  '/Zo', '/d2Zi+')
		
@feature('c', 'cxx')
@after_method('apply_link')
@after_method('process_pch_msvc')
def verify_compiler_options_msvc(self):
	if not 'msvc' in (self.env.CC_NAME, self.env.CXX_NAME):
		return
	
	if Utils.unversioned_sys_platform() != 'win32':
		return
		
	# Verify compiler option (strip all but last for dependant options)
	for t in getattr(self, 'compiled_tasks', []):			
		verify_options_common(t.env)

	# Verify pch_task options (strip all but last for dependant options)
	if hasattr(self, 'pch_task'):
		verify_options_common(self.pch_task.env)

	# Strip unsupported ARCH linker option
	if hasattr(self, 'link_task'):
		del self.link_task.env['ARCH']
	
	
#############################################################################
# Code for auto-recognition of Visual Studio Compiler and Windows SDK Path
# Modified to allow overriding of the MSVC and WinSDK versions to use
#############################################################################

# List of msvc platform pairs (target arch, host arch, compiler-subfolder, dependency-subfolder), ordered from most desirable to least desirable
# Note: Due to cross compilation, we have two possible binaries for each target
all_msvc_platforms = [ ('x64', 'amd64', 'amd64', ''), ('x64', 'x86', 'x86_amd64', '.'), ('x86', 'amd64', 'amd64_x86', 'amd64'), ('x86', 'x86', '.', '') ]

# List of known versions (compiler version, expected Windows SDK), ordered from most preferred to least preferred (order used by auto-detect)
# Note: The expected Windows SDK is the one that's installed with that version by default, and the one we prefer to use for that compiler if we can
all_versions = [ ('11.0', '8.0'), ('14.0', '10.0.10240.0'), ('12.0', '8.1') ]

# Look up the default Windows SDK version to use for the given MSVC version
def get_winsdk_for_msvc_version(version):
	result = []
	for (v, sdk) in all_versions:
		if v == version:
			result += [ sdk ]
	return result

# Look up all supported MSVC version, in order of preference (preference used by auto-detect only)
def get_supported_msvc_versions():
	seen = set()
	return [t[0] for t in all_versions if not (t[0] in seen or seen.add(t[0]))]

# Look up all supported Windows SDK versions
def get_supported_winsdk_versions():
	seen = set()
	return [t[1] for t in all_versions if not (t[1] in seen or seen.add(t[1]))]

# Look up all supported compiler subfolders for a given (target, arch) pair
def get_subfolders_for_target_arch(target, arch):
	result = []
	for (t, a, s, d) in all_msvc_platforms:
		if t == target and a == arch:
			result += [ s ]

		# Since we can run x86 binaries on x64 for windows, consider that as a valid option as well
		# Due to the ordering of the all_msvc_platforms list, these should be last in the result
		if t == target and a == 'x86' and arch == 'amd64':
			result += [ s ]
	return result

# Look up dependency folders (needs to be in PATH when the compiler runs) for a given compiler subfolder
# This subfolder was previously returned as an element of get_subfolders_for_target_arch()
def get_dependency_subfolder_for_compiler_subfolder(subfolder):
	for (t, a, s, d) in all_msvc_platforms:
		if s == subfolder:
			return d;
	return '';

# Given a Windows SDK version, retrieves the requested subfolder (relative to the Windows SDK installation location)
# This should abstract differences between Windows SDK layout changes
# Example, ('8.0', 'include') should give 'v8.0/include', but ('10.0.10240.0', 'include') should give '10/include/10.0.10240.0'
def get_winsdk_subfolder(version, subfolder):
	is_new_sdk_layout = False
	try:
		base_version = version.split('.')[0]
		is_new_sdk_layout = int(base_version) >= 10
	except:
		pass
	if is_new_sdk_layout:
		if 'lib' in subfolder.lower() or 'inc' in subfolder.lower():
			return os.path.join(subfolder, version)
		else:
			return os.path.join(subfolder)
	else:
		if 'lib' in subfolder.lower() and version == '8.0':
			return os.path.join(subfolder, 'win8')
		if 'lib' in subfolder.lower() and version == '8.1':
			return os.path.join(subfolder, 'winv6.3')
	return subfolder

# Given a path to a MSVC folder, finds the folders that will allow compilation for the given target/arch tuple
# If no compiler is found at the given location, an empty dictionary is returned
# The resulting dictionary will contain the following keys:
#	root     (string) Folder parameter passed in, should be the root folder of the compiler (typically, the folder named VC)
#	bin      (string) Folder containing the compiler and linker
#	path     (list of string) Folders to be added to the PATH of the execution environment when using this compiler
#	include  (list of string) Folders to be added to the system include paths when using this compiler
#	lib      (list of string) Folders to be added to the system library paths when using this compiler
#	redist   (list of string) Folders to be considered when copying redistributable binaries for this compiler
def get_msvc_for_target_arch(path, target, arch):
	for subfolder in get_subfolders_for_target_arch(target, arch):
		compiler_bin = os.path.join(path, 'bin', subfolder)
		if os.path.isdir(compiler_bin) and os.path.isfile(os.path.join(compiler_bin, 'cl.exe')):
			# Set up compiler
			result = {}
			result['root'] = path
			result['bin'] = os.path.normpath(compiler_bin)

			# Add VC paths
			result['path'] = [ result['bin'] ]
			dependency_bin = get_dependency_subfolder_for_compiler_subfolder(subfolder)
			if dependency_bin != '':
				result['path'] += [ os.path.normpath(os.path.join(path, 'bin', dependency_bin)) ]
			dependency_ide = os.path.join(path, '..', 'Common7', 'IDE')
			if os.path.isdir(dependency_ide):
				result['path'] += [ os.path.normpath(dependency_ide) ]

			# Add VC includes
			result['include'] = [ os.path.join(path, 'include') ]
			mfc_include = os.path.join(path, 'atlmfc', 'include')
			if os.path.isdir(mfc_include):
				result['include'] += [ mfc_include ]

			# Add VC libraries
			lib_subfolder = '.'
			if target == 'x64':
				lib_subfolder = 'amd64'
			result['lib'] = [ os.path.normpath(os.path.join(path, 'lib', lib_subfolder)) ]
			mfc_lib = os.path.join(path, 'atlmfc', 'lib', lib_subfolder)
			if os.path.isdir(mfc_lib):
				result['lib'] += [ os.path.normpath(mfc_lib) ]

			# Add VC redists
			result['redist'] = {}
			redist = os.path.join(path, 'redist', target)
			if os.path.isdir(redist):
				subdirs = os.listdir(redist)
				for dir in subdirs:
					if os.path.isdir(os.path.join(redist, dir)):
						type = dir.split('.')[-1].lower()
						result['redist'][type] = os.path.join(redist, dir)

			return result

	return {}

# Given a path to a Windows SDK and it's version, finds the folders that will allow compilation for that version of Windows of the given target
# Note: The version number is used by the new Windows 10 layout, which has different include/lib folders per sub-version (ie, 10.0.10240.0)
# If no Windows SDK is found at the given location with that version/target tuple, returns an empty dictionary
# The resulting dictionary will contain the following keys:
#	root     (string) Folder parameter passed in, should be the root of the SDK (typically the folder named with the version number, ie, "8.0", "8.1" or "10")
#	bin      (string) Folder containing the Windows tools (manifest tool and resource compiler)
#	path     (list of string) Folders to be added to the PATH of the execution environment when using this Windows SDK
#	include  (list of string) Folders to be added to the system include paths when using this Windows SDK
#	lib      (list of string) Folders to be added to the system library paths when using this Windows SDK
#	redist   (list of string) Folders to be considered when copying redistributable binaries for this Windows SDK
def get_winsdk_for_target(path, version, target):
	include = os.path.join(path, get_winsdk_subfolder(version, 'include'))
	lib = os.path.join(path, get_winsdk_subfolder(version, 'lib'))
	bin = os.path.join(path, get_winsdk_subfolder(version, 'bin'), target)
	if os.path.isdir(include) and os.path.isdir(lib) and os.path.isdir(bin):
		
		# Test potential paths for existence
		includes = [ os.path.join(include, 'um'), os.path.join(include, 'ucrt'), os.path.join(include, 'shared'), os.path.join(include, 'winrt') ]
		libs = [ os.path.join(lib, 'um', target), os.path.join(lib, 'ucrt', target) ]
		includes = [ os.path.normpath(x) for x in includes if os.path.isdir(x) ]
		libs = [ os.path.normpath(x) for x in libs if os.path.isdir(x) ]
		if len(includes) != 0 and len(libs) != 0:
			# Set up SDK
			result = {}
			result['root'] = path
			result['bin'] = os.path.normpath(bin)

			# Add SDK paths (we could add bin here, but it shouldn't be necessary)
			result['path'] = [ ]

			# Add SDK includes
			result['include'] = includes

			# Add SDK libraries
			result['lib'] = libs

			# Add SDK redists
			result['redist'] = {}
			d3d_redist = os.path.join(path, get_winsdk_subfolder(version, 'redist'), 'D3D', target)
			if os.path.isdir(d3d_redist):
				result['redist']['d3d'] = d3d_redist
			dbg_redist = os.path.join(path, get_winsdk_subfolder(version, 'debuggers'), target)
			if os.path.isdir(dbg_redist):
				result['redist']['dbghelp'] = dbg_redist

			return result

	return {}

# List of target/arch tuples for which we already output some diagnostics
# This is used to prevent spamming the output many times for each configuration of the saem target/arch tuple
# TODO: Maybe there is a better way for this?
HAS_DIAGNOSED = []

@conf
def load_msvc_compiler(conf, target, arch):
	"""
	Loads the MSVC compiler for the given target/arch tuple into the current environment
	"""
	v = conf.env

	# Check if we already have diagnosed problem output for this target/arch tuple
	# Note: Even if we diagnose a potential problem, this is no longer a fatal error
	# Any fatal errors will already have occurred during the detect function call above
	global HAS_DIAGNOSED
	has_diagnosed = True
	if not target + '_' + arch in HAS_DIAGNOSED:
		HAS_DIAGNOSED += [ target + '_' + arch ]
		has_diagnosed = False
	verbose = conf.is_option_true('auto_detect_verbose') and not has_diagnosed

	# Get the compiler and SDK to use
	if not conf.is_option_true('auto_detect_compiler'):
		sdk, compiler = conf.detect_bootstrapped_msvc_compiler(target, arch)
		type = 'bootstrapped'
		if verbose:
			print '[NOTE] Ignoring --auto-detect-verbose when --auto-detect-compiler is disabled'
	else:
		sdk, compiler = conf.auto_detect_msvc_compiler(target, arch, verbose)
		type = 'auto-detected'

	if not has_diagnosed:
		# Show diagnostics for this target/arch pair
		print '[NOTE] Using %s MSVC %s (%s) with Windows SDK %s' % (type, compiler['version'], os.path.split(compiler['bin'])[1], sdk['version'])

	# Apply general paths
	v['PATH'] = compiler['path'] + sdk['path']
	v['INCLUDES'] = compiler['include'] + sdk['include']
	v['LIBPATH'] = compiler['lib'] + sdk['lib']

	# Apply programs to use for build
	v['AR'] = os.path.join(compiler['bin'], 'lib.exe')
	v['CC'] = v['CXX'] = os.path.join(compiler['bin'], 'cl.exe')
	v['LINK'] = v['LINK_CC'] = v['LINK_CXX'] = os.path.join(compiler['bin'], 'link.exe')
	v['MT'] = os.path.join(sdk['bin'], 'mt.exe')
	v['WINRC'] = os.path.join(sdk['bin'], 'rc.exe')
	
	# Verify the tools exist
	if not os.path.isfile(v['AR']):
		conf.fatal('C/C++ archiver not found at ' + v['AR'])
	if not os.path.isfile(v['CC']):
		conf.fatal('C/C++ compiler not found at ' + v['CC'])
	if not os.path.isfile(v['LINK']):
		conf.fatal('C/C++ linker not found at ' + v['LINK'])
	if not os.path.isfile(v['MT']):
		conf.fatal('Windows Manifest tool not found at ' + v['MT'])
	if not os.path.isfile(v['WINRC']):
		conf.fatal('Windows Resource Compiler tool not found at ' + v['WINRC'])

	# Apply MSVC paths (for later use by MSVC specific build steps)
	v['MSVC_PATH'] = compiler['root']
	v['MSVC_VERSION'] = compiler['version']
	v['MSVC_COMPILER'] = v['CC_NAME'] = v['CXX_NAME'] = 'msvc'
	v['MSVC_TARGETS'] = [ target ]

	# Apply WINSDK paths (for later use by Windows specific build steps)
	v['WINSDK_PATH'] = sdk['root']
	v['WINSDK_VERSION'] = sdk['version']
	
	# Apply merged redist path dictionaries
	v['REDIST_PATH'] = {}
	v['REDIST_PATH'].update(compiler['redist'])
	v['REDIST_PATH'].update(sdk['redist'])

	# Add MSVC required path to global path
	v.env = os.environ.copy()
	if not 'PATH' in v.env:
		v.env['PATH'] = ''
	v.env['PATH'] = ';'.join(v['PATH']) + ';' + v.env['PATH']

	# Strip duplicate paths
	v.env['PATH'] =';'.join(set(v.env['PATH'].split(';')))

@conf
def detect_bootstrapped_msvc_compiler(conf, target, arch):
	"""
	Used to set up compiler in Code/SDKS
	This is used internally at Crytek
	"""

	# Setup Tools for MSVC Compiler for internal projects using bootstrap/3rdParty
	msvc_compiler_folder = conf.CreateRootRelativePath('Code/SDKs/Microsoft Visual Studio Compiler')
	if not os.path.isdir(msvc_compiler_folder):
		conf.fatal('[ERROR] Unable to find MSVC directory at: %s' % msvc_compiler_folder)

	# Find MSVC version by looking inside redist folder
	msvc_redist_path = os.path.join(msvc_compiler_folder, 'redist', target)
	if conf.options.force_msvc.lower() == 'auto':
		for redist_folder in os.walk(msvc_redist_path, followlinks=True).next()[1]:
			msvc_version = re.findall(r"(VC[\d]*?\d+)", redist_folder)[0]
			msvc_dot_version = msvc_version[2:-1] + '.' + msvc_version[-1]
			break
	else:
		msvc_dot_version = conf.options.force_msvc.lower()

	# Find WinSDK version
	# Note: We assume here that the bootstrapped MSVC is in the preferred list
	if conf.options.force_winsdk.lower() == 'auto':
		sdk_version = get_winsdk_for_msvc_version(msvc_dot_version)[0]
	else:
		sdk_version = conf.options.force_winsdk.lower()

	# Note: Bootstrapped SDKs are in a non-default folder name (with a v prefix) for 8.0 and 8.1
	# This function accounts for these special cases so we don't need to re-sync the whole SDKs
	# For licensees, this doesn't matter since they should be using auto-detect mode
	sdk_folder = conf.CreateRootRelativePath('Code/SDKs/Microsoft Windows SDK')
	if sdk_version == '8.0':
		sdk_folder = os.path.join(sdk_folder, 'v8.0')
	elif sdk_version == '8.1':
		sdk_folder = os.path.join(sdk_folder, 'v8.1')
	else:
		sdk_folder = os.path.join(sdk_folder, sdk_version[:sdk_version.index('.')])
	if not os.path.isdir(sdk_folder):
		conf.fatal('[ERROR] Unable to find WinSDK directory at: %s' % ms_sdk_folder)

	# Get the compiler information
	compiler = get_msvc_for_target_arch(msvc_compiler_folder, target, arch)
	compiler['version'] = msvc_dot_version

	# Get the Windows SDK information
	sdk = get_winsdk_for_target(sdk_folder, sdk_version, target)
	sdk['version'] = sdk_version

	return sdk, compiler

@conf
def auto_detect_msvc_compiler(conf, target, arch, verbose):
	"""
	Pick the MSVC and WinSDK to use for the given target/arch tuple from the installed versions
	Uses --force-msvc and --force-winsdk to override automatic picks, if defined
	"""
	sdks, compilers = conf.get_msvc_versions(target, arch)

	# Pick which compiler to use out of the available ones
	force_msvc = conf.options.force_msvc.lower()
	compiler = {}
	if force_msvc != 'auto':
		if not force_msvc in compilers:
			conf.fatal('[ERROR]: The --force-msvc value %s could not be satisfied, the following are supported on this system: %s' % (force_msvc, ', '.join(compilers.keys())))
		else:
			compiler = compilers[force_msvc]
			if verbose:
				print '[NOTE] Picking MSVC %s because --force-msvc is set' % force_msvc
	else:
		for version in get_supported_msvc_versions():
			if version in compilers:
				compiler = compilers[version]
				if verbose:
					print '[NOTE] Picking MSVC %s because it is the first supported by preference from list: %s' % (version, ', '.join(get_supported_msvc_versions()))
				break

	# Pick the first available compiler and hope for the best
	# Note: This is a fallback so we at least attempt compilation, but it may just straight up fail
	# In general, if support for a compiler is needed, it should be added to the all_versions global in favor of compiling with --force-msvc & --force-winsdk
	if len(compiler) == 0:
		if len(compilers) != 0:
			compiler = compilers.values()[0]
			Logs.warn('[WARNING]: Picking MSVC %s, which is not (yet) supported by WAF. Supported versions: %s' % (compilers.keys()[0] % ', '.join(get_supported_msvc_versions())))
			print '[NOTE] Consider setting --force-msvc to override this warning, available list: %s' % ', '.join(compilers.keys())
		if not verbose:
			print '[NOTE] Consider running "configure" with --auto-detect-verbose turned on for additional information'
		if len(compiler) == 0:
			conf.fatal('[ERROR]: No installed MSVC compiler could be found for target (%s, %s)' % (target, arch))
	msvc_version = compiler['version']

	# Pick which SDK to use
	force_winsdk = conf.options.force_winsdk.lower()
	sdk = {}
	if force_winsdk != 'auto':
		if not force_winsdk in sdks:
			conf.fatal('[ERROR]: The --force-winsdk value %s could not be satisfied, the following are supported on this system: %s' % (force_winsdk, ', '.join(sdks.keys())))
		else:
			sdk = sdks[force_winsdk]
			if verbose:
				print '[NOTE] Picking Windows SDK %s because --force-winsdk is set' % force_winsdk
	else:
		preferred_versions = get_winsdk_for_msvc_version(msvc_version)
		for preferred_version in preferred_versions:
			if preferred_version in sdks:
				sdk = sdks[preferred_version]
				if verbose:
					print '[NOTE] Picking Windows SDK %s because it is preferred by MSVC %s' % (preferred_version, msvc_version)

	# Pick the first supported SDK, or just a random one
	# Note: This is a fallback so we at least attempt compilation, but it may just straight up fail
	# In general, if support for a compiler is needed, it should be added to the all_versions global in favor of compiling with --force-msvc & --force-winsdk
	if len(sdk) == 0:
		version = ''
		for other_version in get_supported_winsdk_versions():
			if other_version in sdks:
				version = other_version
				break;
		if version == '' and len(sdks) != 0:
			version = sdks.keys()[0]
		if version in sdks:
			sdk = sdks[version]
			Logs.warn('[WARNING]: Picking Windows SDK %s, which is not supported in combination with MSVC %s' % (version, msvc_version))
			print '[NOTE] WAF supports the following Windows SDKs for use by MSVC %s: %s' % (msvc_version, ', '.join(get_winsdk_for_msvc_version(msvc_version)))
			print '[NOTE] Consider setting --force-winsdk to override this warning, available list: %s' % ', '.join(sdks.keys())
		if not verbose:
			print '[NOTE] Consider running "configure" with --auto-detect-verbose turned on for additional information'
		if len(sdk) == 0:
			conf.fatal('[ERROR]: No installed Windows SDK could be found for target (%s, %s)' % (target, arch))

	return sdk, compiler

# Don't access directly, these are (potentially uninitialized) caches
# Instead use get_msvc_versions function
MSVC_INSTALLED_VERSIONS = {}
WINSDK_INSTALLED_VERSIONS = {}

@conf
def get_msvc_versions(conf, target, arch):
	"""
	Requests from cache all the installed MSVC and WinSDK that support the given target/arch tuple
	Returns two dictionaries of key (version) and value (installation)
	The type of the returned installation is the same as returned from get_winsdk_for_target() and get_msvc_for_target_arch()
	"""

	# Configuration
	verbose = conf.is_option_true('auto_detect_verbose')
	min_version = float(conf.options.minimum_msvc_compiler)

	# Get the WinSDK versions installed on this machine, and cache it for future inspection
	global WINSDK_INSTALLED_VERSIONS
	if len(WINSDK_INSTALLED_VERSIONS) == 0:
		WINSDK_INSTALLED_VERSIONS['all'] = gather_wsdk_versions(verbose)
		if len(WINSDK_INSTALLED_VERSIONS['all']) == 0:
			conf.fatal('[ERROR] No Windows SDK installed on this machine')

	# Get the MSVC versions installed on this machine, and cache it for future inspection
	global MSVC_INSTALLED_VERSIONS
	if len(MSVC_INSTALLED_VERSIONS) == 0:
		MSVC_INSTALLED_VERSIONS['all'] = gather_msvc_versions(verbose, min_version)
		if len(MSVC_INSTALLED_VERSIONS['all']) == 0:
			conf.fatal('[ERROR] No MSVC compiler installed on this machine')

	# Get the SDKs for this target
	sdks = WINSDK_INSTALLED_VERSIONS.get(target, {})
	if len(sdks) == 0:
		suitable = []
		unsuitable = []
		for (version, path) in WINSDK_INSTALLED_VERSIONS['all'].items():
			sdk = get_winsdk_for_target(path, version, target)
			if len(sdk) != 0:
				sdk['version'] = version
				sdks[version] = sdk
				suitable += [ version ]
			else:
				unsuitable += [ version ]
		if verbose:
			print '[NOTE] There are %u suitable Windows SDKs for target %s: %s (unsuitable: %u)' % (len(suitable), target, ', '.join(suitable), len(unsuitable))
		if len(sdks) == 0:
			conf.fatal('[ERROR] None of the installed Windows SDK provides support for target %s (installed: %s)' % (target, ', '.join(WINSDK_INSTALLED_VERSIONS['all'].keys())))
		WINSDK_INSTALLED_VERSIONS[target] = sdks

	# Get the MSVCs for this target/arch tuple
	msvcs = MSVC_INSTALLED_VERSIONS.get(target + '@' + arch, {})
	if len(msvcs) == 0:
		suitable = []
		unsuitable = []
		for (version, path) in MSVC_INSTALLED_VERSIONS['all'].items():
			msvc = get_msvc_for_target_arch(path, target, arch)
			if len(msvc) != 0:
				msvc['version'] = version
				msvcs[version] = msvc
				suitable += [ version ]
			else:
				unsuitable += [ version ]
		if verbose:
			print '[NOTE] There are %u suitable MSVC for target (%s, %s): %s (unsuitable: %u)' % (len(suitable), target, arch, ', '.join(suitable), len(unsuitable))
		if len(msvcs) == 0:
			conf.fatal('[ERROR] None of the installed MSVC provides support for target %s (host: %s, installed %s)' % (target, arch, ','.join(MSVC_INSTALLED_VERSIONS['all'].keys())))
		MSVC_INSTALLED_VERSIONS[target + '@' + arch] = msvcs

	return sdks, msvcs

def gather_msvc_versions(verbose, min_version):
	"""
	Use registry to collect all the MSVC installed versions
	The resulting dictionary will contain key (version) and value (path) for each MSVC in the registry
	"""

	# Collect registry keys for MSVC installations
	# Note: since we insert into dictionary, the last vcver type takes effect (this is intended, since we want to avoid Express if we can)
	vc_paths = {}
	detected_versions = {}
	discarded_no_vc = 0
	discarded_no_dir = 0
	discarded_sku = 0
	discarded_version = 0
	version_pattern = re.compile('^(\d\d\.\d)(Exp)?$')
	for vcver, vcvar in [('VCExpress','Exp'), ('VisualStudio','')]: 
		try:
			prefix = 'SOFTWARE\\Wow6432Node\\Microsoft\\' + vcver
			all_versions = Utils.winreg.OpenKey(Utils.winreg.HKEY_LOCAL_MACHINE, prefix)
		except:
			try:
				prefix = 'SOFTWARE\\Microsoft\\' + vcver
				all_versions = Utils.winreg.OpenKey(Utils.winreg.HKEY_LOCAL_MACHINE, prefix)
			except:
				continue

		index = 0
		while 1:
			# Check if this is a Visual Studio installation by pattern match
			try:
				version = Utils.winreg.EnumKey(all_versions, index)
			except:
				break
			index = index + 1
			match = version_pattern.match(version)
			if not match:
				continue
			else:
				versionnumber = match.group(1)

			# Find the VC++ installation directory for this Visual Studio
			reg = prefix + '\\' + version
			try:
				try:
					msvc_version = Utils.winreg.OpenKey(Utils.winreg.HKEY_LOCAL_MACHINE, reg + "\\Setup\\VC")
					reg += "\\Setup\\VC"
				except:
					msvc_version = Utils.winreg.OpenKey(Utils.winreg.HKEY_LOCAL_MACHINE, reg + "\\Setup\\Microsoft Visual C++")
					reg += "\\Setup\\Microsoft Visual C++"
				path, type = Utils.winreg.QueryValueEx(msvc_version, 'ProductDir')
				path = os.path.abspath(path.encode('utf8', 'ignore'))
			except:
				discarded_no_vc += 1
				continue

			# Verify the path is present on disk
			if not os.path.isdir(path):
				discarded_no_dir += 1
				continue

			# Check that the minimum version requirement is specified
			if float(versionnumber) < min_version:
				if verbose:
					print '[NOTE] Ignoring MSVC %s because the minimum version is set to %.1f (registry: HKLM\\%s, path: %s)' % (versionnumber, min_version, reg, path)
				discarded_version += 1
				continue;

			# Apply the detected version
			vc_paths[versionnumber] = path

			# Print discarded SKUs due to duplicate version numbers
			if verbose:
				print '[NOTE] Detected MSVC ' + versionnumber + ' (registry: HKLM\\' + reg + ', path: ' + path + ')'
				if versionnumber in detected_versions:
					discarded_sku += 1
					previous_vcver, previous_path = detected_versions[versionnumber]
					print '[NOTE] Discarding previously detected MSVC %s because SKU %s is a better match than SKU %s' % (versionnumber, vcver, previous_vcver)
				detected_versions[versionnumber] = (vcver, path)

	# Print statistics
	if verbose:
		count = len(vc_paths)
		if count == 0:
			print '[NOTE] No Visual C++ installations found, check the selected packages in your existing Visual Studio installation, or re-install'
		else:
			print '[NOTE] Visual C++ detection completed: %u found, %u too low version, %u VS without VC, %u duplicate versions, %u not present on disk' % (count, discarded_version, discarded_no_vc, discarded_sku, discarded_no_dir)
		if discarded_version != 0:
			print '[NOTE] One or more Visual C++ versions were ignored due to --minimum-msvc-compiler, consider changing this value if you are sure you want to use them'
	return vc_paths

def gather_wsdk_versions(verbose):
	"""
	Use winreg to collect all the WinSDK installed versions
	The resulting dictionary will contain key (version) and value (path) for each Windows SDK in the registry
	"""

	# Get root node of Windows SDKs
	try:
		reg = 'SOFTWARE\\Wow6432node\\Microsoft\\Microsoft SDKs\\Windows'
		all_versions = Utils.winreg.OpenKey(Utils.winreg.HKEY_LOCAL_MACHINE, reg)
	except:
		try:
			reg = 'SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows'
			all_versions = Utils.winreg.OpenKey(Utils.winreg.HKEY_LOCAL_MACHINE, reg)
		except:
			if verbose:
				print '[NOTE] No Windows SDKs installations found, check the selected packages in your existing Visual Studio installation, or re-install'
			return

	# Read Windows SDKs from registry
	sdks = {}
	index = 0
	discarded_no_manifest = 0
	identity_pattern = re.compile('^\w+,\s*Version=([\d\.]+)$')
	while 1:
		# Check if key matches version
		try:
			version = Utils.winreg.EnumKey(all_versions, index)
		except WindowsError:
			break
		index = index + 1

		try:
			msvc_version = Utils.winreg.OpenKey(all_versions, version)
			path, type = Utils.winreg.QueryValueEx(msvc_version, 'InstallationFolder')
			path = os.path.abspath(path.encode('utf8', 'ignore'))
			manifest_file = os.path.join(path, 'SDKManifest.xml')
			try:
				xml_tree = ET.parse(manifest_file).getroot()
				match = identity_pattern.match(xml_tree.attrib['PlatformIdentity'])
				sdk_version = match.group(1)
				sdks[sdk_version] = path

				if verbose:
					print '[NOTE] Detected Windows SDK %s (registry: HKLM\\%s\\%s, path: %s)' % (sdk_version, reg, version, path)
			except:
				discarded_no_manifest += 1
		except:
			continue

	# Print statistics
	if verbose:
		count = len(sdks)
		print '[NOTE] Windows SDK detection completed: %u found, %u without valid SDKManifest.xml' % (count, discarded_no_manifest)
	return sdks

def copy_redist_files(self, package, filter_list):
	"""
	Copies redistributable files matching a given filter list from a redistributable package
	The function returns True if at least one files will be copied, False otherwise
	Note: If the filter list is an empty list, all files from the package are copied
	"""
	folder = ''
	if 'REDIST_PATH' in self.env:
		if package in self.env['REDIST_PATH']:
			folder = self.env['REDIST_PATH'][package]

	if len(folder) == 0:
		return False

	try:
		files = os.listdir(folder)
	except:
		return False

	output_folder = self.bld.get_output_folders(self.bld.env['PLATFORM'], self.bld.env['CONFIGURATION'])[0]
	if hasattr(self, 'output_sub_folder'):
		if os.path.isabs(self.output_sub_folder):
			output_folder = self.bld.root.make_node(self.output_sub_folder)
		else:
			output_folder = output_folder.make_node(self.output_sub_folder)			

	any_files = False
	for file in files:
		copy = False
		for filter in filter_list:
			if filter in file.lower():
				copy = True
		if len(filter_list) == 0:
			copy = True
		if copy and os.path.isfile(os.path.join(folder, file)):
			self.create_task('copy_outputs', self.bld.root.make_node(os.path.join(folder, file)), output_folder.make_node(file))
			any_files = True
	return any_files

@feature('cxxprogram', 'cprogram')
@after_method('apply_link')
def copy_msvc_crt_binaries(self):

	if self.env['PLATFORM'] == 'project_generator' or not 'REDIST_PATH' in self.env or not 'msvc' in (self.env.CC_NAME, self.env.CXX_NAME):
		return

	# Note: Only the /MD libraries are redistributable, so any other config we can ignore entirely
	if not '/MD' in self.env['CXXFLAGS']:
		return

	# Copy CRT (not the debug one, as not re-distributable) 
	# Good description can be found here: http://stackoverflow.com/questions/14749662/microsoft-visual-studio-c-c-runtime-library-static-dynamic-linking
	any_copied = copy_redist_files(self, 'crt', [ 'msvcr', 'msvcp', 'vcruntime', 'concrt' ])

	if not any_copied:
		Logs.warn('[WARNING] No CRT DLLs to copy found, maybe redistributables were not installed with Visual Studio?')

@feature('copy_d3d_compiler')
@after_method('apply_link')
def copy_msvc_d3d_compiler(self):

	if self.env['PLATFORM'] == 'project_generator' or not 'REDIST_PATH' in self.env or not 'msvc' in (self.env.CC_NAME, self.env.CXX_NAME):
		return

	any_copied = copy_redist_files(self, 'd3d', [ 'd3dcompiler' ])

	if not any_copied:
		Logs.warn('[WARNING] No D3D DLLs to copy found, maybe redistributables were not installed with Windows SDK?')

@feature('copy_mfc_binaries')
@after_method('apply_link')
def copy_msvc_mfc_binaries(self):

	if self.env['PLATFORM'] == 'project_generator' or not 'REDIST_PATH' in self.env or not 'msvc' in (self.env.CC_NAME, self.env.CXX_NAME):
		return

	# Note: Only the /MD libraries are redistributable, so any other config we can ignore entirely
	if not '/MD' in self.env['CXXFLAGS']:
		return

	# Note: We want to skip the 1X0u.dll here, because they are Unicode variants, and we want the MBCS ones
	# Known issue: There doesn't seem to be any redist for VS2015's non-Unicode MFC available
	# From the localized MFC, just grab only English-US, the other languages are not supported (and it would lead to half-localized sandbox)
	any_core_copied = copy_redist_files(self, 'mfc', [ '0.dll' ])
	any_loc_copied = copy_redist_files(self, 'mfcloc', [ 'enu' ])

	if not any_core_copied:
		Logs.warn('[WARNING] No MFC MBCS DLLs to copy found, maybe redistributables were not installed with Visual Studio?')
	elif not any_loc_copied:
		Logs.warn('[WARNING] No localized MFC DLLs to copy found, maybe localized MFC redistributables were not installed with Visual Studio?')

@feature('copy_dbghelp')
@after_method('apply_link')
def copy_copy_wsdk_dbghelp(self):

	if self.env['PLATFORM'] == 'project_generator' or not 'REDIST_PATH' in self.env or not 'msvc' in (self.env.CC_NAME, self.env.CXX_NAME):
		return

	any_copied = copy_redist_files(self, 'dbghelp', [ 'dbghelp.dll' ])

	if not any_copied:
		Logs.warn('[WARNING] No dbghelp.dll found in Windows SDK, maybe debuggers were not installed with Windows SDK?')
