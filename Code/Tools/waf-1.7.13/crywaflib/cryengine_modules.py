#############################################################################
## Crytek Source File
## Copyright (C) 2013, Crytek Studios
##
## Creator: Christopher Bolte
## Date: Oct 31, 2013
## Description: WAF  based build system
#############################################################################


###############################################################################
from waflib.Configure import conf
from waflib.TaskGen import feature, before_method, after_method
from waflib import Utils, Logs, Errors
import os
import copy
import re	
import itertools
from collections import Iterable
from waflib import Context

def _preserved_order_remove_duplicates(seq):
	seen = set()
	seen_add = seen.add
	return [ x for x in seq if not (x in seen or seen_add(x))]
		
@conf
def GetConfiguration(ctx, target):	
	if target in ctx.env['CONFIG_OVERWRITES']:		
		return ctx.env['CONFIG_OVERWRITES'][target]
	return ctx.env['CONFIGURATION']
	
@conf
def get_platform_list(self, platform):
	"""
	Util-function to find valid platform aliases for the current platform
	"""
	supported_platforms = self.get_supported_platforms()

	# Create all possible platform subsets e.g. ['android', 'android_arm', 'android_arm_gcc']
	# For all possible platforms
	if platform in ('project_generator', 'cppcheck', []): # if empty platform or on of the other in the list
		ret_value = set()
		for concrete_platform in self.get_all_platforms():
			platform_name = ""
			for platform_subset in concrete_platform.split('_'):
				platform_name = '%s_%s' % (platform_name, platform_subset) if platform_name else platform_subset
				ret_value.add(platform_name)
		ret_value = _preserved_order_remove_duplicates(list(ret_value)) # strip duplicate
		
	# For the given platform
	else:
		ret_value = []
		platform_name = ""
		for platform_subset in platform.split('_'):
			platform_name = '%s_%s' % (platform_name, platform_subset) if platform_name else platform_subset
			if any(platform_name in s for s in supported_platforms):
				ret_value += [platform_name]

	return ret_value	
	
def SanitizeInput(ctx, kw):
	"""
	Util-function to sanitize all inputs: add missing fields and make sure all fields are list
	"""
	def _SanitizeInput(kw, entry):
		"""
		Private Helper function to sanitize a single entry
		"""	
		assert( isinstance(entry, str) )
		if not entry in kw:
			kw[entry] = []

		if not isinstance(kw[entry],list):
			kw[entry] = [ kw[entry] ]

	for entry in 'output_file_name files additional_settings use_module export_definitions meta_includes file_list winres_includes winres_defines defines features java_source lib libpath linkflags use cflags cxxflags includes framework module_extensions'.split():
	
		# Base entry
		_SanitizeInput(kw, entry)
		
		# By Configuration entry
		for conf in ctx.get_supported_configurations():
			_SanitizeInput(kw, conf + '_' + entry)
		
		# By Platform entry
		platform_list = ctx.get_platform_list( [] )
		for platform in platform_list:
			_SanitizeInput(kw, platform + '_' + entry)

		# By Platform,Configuration entry
		for platform in platform_list:
			for conf in ctx.get_supported_configurations():
				_SanitizeInput(kw, platform + '_' + conf + '_' + entry)

	# Process module-provided settings
	for setting in kw.setdefault('module_provides', dict()).keys():
		_SanitizeInput(kw['module_provides'], setting)
		
	# Recurse for additional settings
	for setting in kw['additional_settings']:
		SanitizeInput(ctx, setting)

		
def RegisterVisualStudioFilter(ctx, kw):
	"""
	Util-function to register each provided visual studio filter parameter in a central lookup map
	"""
	if not 'vs_filter' in kw:
		ctx.fatal('Mandatory "vs_filter" task generater parameter missing in %s/wscript' % ctx.path.abspath())
		
	if not hasattr(ctx, 'vs_project_filters'):
		ctx.vs_project_filters = {}
			
	ctx.vs_project_filters[ kw['target' ] ] = kw['vs_filter']	
	
def AssignTaskGeneratorIdx(ctx, kw):
	"""
	Util-function to assing a unique idx to prevent concurrency issues when two task generator output the same file.
	"""
	if not hasattr(ctx, 'index_counter'):		ctx.index_counter = 0
	if not hasattr(ctx, 'index_map'):
		ctx.index_map = {}
		
	# Use a path to the wscript and the actual taskgenerator target as a unqiue key
	key = ctx.path.abspath() + '___' + kw['target']
	
	if key in ctx.index_map:
		kw['idx'] = ctx.index_map.get(key)
	else:
		ctx.index_counter += 1	
		kw['idx'] = ctx.index_map[key] = ctx.index_counter
		
	kw['features'] += ['parse_vcxproj']
		
def SetupRunTimeLibraries(ctx, kw, overwrite_settings = None):
	"""
	Util-function to set the correct flags and defines for the runtime CRT (and to keep non windows defines in sync with windows defines)
	By default CryEngine uses the "Multithreaded, dynamic link" variant (/MD)
	"""
	runtime_crt = 'dynamic'						# Global Setting
	if overwrite_settings:						# Setting per Task Generator Type
		runtime_crt = overwrite_settings
	if kw.get('force_static_crt', False):		# Setting per Task Generator
		runtime_crt = 'static'
	if kw.get('force_dynamic_crt', False):		# Setting per Task Generator
		runtime_crt = 'dynamic'
		
	if runtime_crt != 'static' and runtime_crt != 'dynamic':
		ctx.fatal('Invalid Settings: "%s" for runtime_crt' % runtime_crt )
		
	crt_flag = []
	config = ctx.GetConfiguration(kw['target'])			

	if runtime_crt == 'static':
		kw['defines'] 	+= [ '_MT' ]
		if ctx.env['CC_NAME'] == 'msvc':
			if config == 'debug':
				crt_flag = [ '/MTd' ]
			else:
				crt_flag	= [ '/MT' ]			
	else: # runtime_crt == 'dynamic':
		kw['defines'] 	+= [ '_MT', '_DLL' ]	
		if ctx.env['CC_NAME'] == 'msvc':
			if config == 'debug':
				crt_flag = [ '/MDd' ]
			else:
				crt_flag	= [ '/MD' ]		
	
	kw['cflags']	+= crt_flag
	kw['cxxflags']	+= crt_flag
	
	
def TrackFileListChanges(ctx, kw):
	"""
	Util function to ensure file lists are correctly tracked regardless of current target platform
	"""	
	def _to_list( value ):
		""" Helper function to ensure a value is always a list """
		if isinstance(value,list):
			return value
		return [ value ]
	
	files_to_track = []
	kw['waf_file_entries'] = []
	
	# Collect all file list entries
	for (key,value) in kw.items():	
		if 'file_list' in key:
			files_to_track += _to_list(value)
		# Collect potential file lists from addional options
		if 'additional_settings' in key:
			for settings_container in kw['additional_settings']:
				for (key2,value2) in settings_container.items():
					if 'file_list' in key2:
						files_to_track += _to_list(value2)
		
	# Remove duplicates
	files_to_track = list(set(files_to_track)) 
	
	# Add results to global lists
	for file in files_to_track:
		file_node = ctx.path.make_node(file)	
		if not hasattr(ctx, 'addional_files_to_track'):
			ctx.addional_files_to_track = []	
		ctx.addional_files_to_track += [ file_node ]		
		kw['waf_file_entries'] += [ file_node ]

def LoadFileLists(ctx, kw, file_lists):
	"""
	Util function to extract a list of needed source files, based on uber files and current command
	It expects that kw['file_list'] points to a valid file, containing a JSON file with the following mapping:
	Dict[ <UberFile> -> Dict[ <Project Filter> -> List[Files] ] ]
	"""	
	def _MergeFileList(in_0, in_1):	
		""" Merge two file lists """
		result = dict(in_0)
		
		for (uber_file,project_filter) in in_1.items():				
			for (filter_name,file_list) in project_filter.items():				
				for file in file_list:				
					if not uber_file in result:						
						result[uber_file] = {}					
					if not filter_name in result[uber_file]:						
						result[uber_file][filter_name] = []									
					result[uber_file][filter_name].append(file)	
		return result
		
	def _DisableUberFile(ctx, project_filter_list, files_marked_for_exclusion):
		for (filter_name, file_list) in project_filter_list.items():				
			if any(ctx.path.make_node(file).abspath().lower() in files_marked_for_exclusion for file in file_list): # if file in exclusion list			
				return True
		return False
	
	task_generator_files		= set() # set of all files in this task generator (store as abspath to be case insenstive)
	
	file_to_project_filter	= {}
	uber_file_to_file_list	= {}
	file_list_to_source			= {}
	file_list_content				= {}
	
	source_files						= []
	no_uber_file_files			= []
	header_files						= []
	darwin_source_files			= []
	java_source_files       = []
	qt_source_files					= []
	resource_files					= []
	uber_file_list					= []
	other_files							= []
	uber_file_relative_list	= []
	
	target 									= kw['target']
	found_pch								= False
	pch_file								= kw.get('pch', '')
	platform 								= ctx.env['PLATFORM']
	uber_file_folder 				= ctx.root.make_node(Context.launch_dir).make_node('BinTemp/uber_files/' + target)

	# Apply project override
	disable_uber_files_for_project = ctx.get_project_overrides(target).get('exclude_from_uber_file', False)
											
	files_marked_for_uber_file_exclusion = []	
	if not disable_uber_files_for_project:
		for key, value in ctx.get_file_overrides(target).iteritems():
			if value.get('exclude_from_uber_file', False):
				files_marked_for_uber_file_exclusion.append(key)

	# Load file lists	and build all needed lookup lists
	for file_list_file in file_lists:
		# Read *.waf_file from disc
		file_list					= ctx.read_file_list( file_list_file )
		file_list_content = _MergeFileList(file_list_content ,file_list)

		# Build various mappings/lists based in file just
		for (uber_file, project_filter_list) in file_list.items():

			# Disable uber file usage if defined by override parameter
			disable_uber_file = disable_uber_files_for_project or _DisableUberFile(ctx, project_filter_list, files_marked_for_uber_file_exclusion)
			
			if disable_uber_file:
				Logs.debug('[Option Override] - %s - Disabled uber file "%s"' %(target, uber_file))
				
			generate_uber_file = uber_file != 'NoUberFile' and not disable_uber_file
			if generate_uber_file:
				# Collect Uber file related informations	
				uber_file_node 			= uber_file_folder.make_node(uber_file)			
				uber_file_relative 	= uber_file_node.path_from(ctx.path)	
				
				if uber_file in uber_file_list:
					ctx.cry_file_error('[%s] UberFile "%s" was specifed twice. Please choose a different name' % (kw['target'], uber_file), file_list_file)
					
				task_generator_files.add(uber_file_node.abspath().lower())
				uber_file_list 										+= [ uber_file ]
				uber_file_relative_list						+= [ uber_file_relative ]
				file_to_project_filter[uber_file_node.abspath()] = 'Uber Files'
			
			for (filter_name, file_entries) in project_filter_list.items():				
				for file in file_entries:
					file_node = ctx.path.make_node(file)
					
					task_generator_files.add(file_node.abspath().lower())
					# Collect per file information		
					if file == pch_file:
						# PCHs are not compiled with the normal compilation, hence don't collect them
						found_pch = True							
						
					elif file.endswith('.c') or file.endswith('.C'):
						source_files 									+= [ file ]
						if not generate_uber_file:
							no_uber_file_files 					+= [ file ]
							
					elif file.endswith('.cpp') or file.endswith('.CPP'):
						source_files 									+= [ file ]
						if not generate_uber_file:
							no_uber_file_files 					+= [ file ]

					elif 'android' in ctx.env['PLATFORM'] and file.endswith('.s'): #TODO this should have a real handling for assembly files
						source_files 								+= [ file ]
						no_uber_file_files 					+= [ file ]
						
					elif file.endswith('.java'):
						java_source_files 						+= [ file ]
							
					elif file.endswith('.mm'):
						darwin_source_files 					+= [ file ]
						
					elif file.endswith('.ui') or file.endswith('.qrc'):
						qt_source_files 							+= [ file ]
					
					elif file.endswith('.h') or file.endswith('.H'):
						header_files 									+= [ file ]
					elif file.endswith('.hpp') or file.endswith('.HPP'):
						header_files 									+= [ file ]
					
					elif file.endswith('.rc') or file.endswith('.r'):
						resource_files								+= [ file ]
						
					else:
						other_files										+= [ file ]
						
					# Build file name -> Visual Studio Filter mapping
					file_to_project_filter[file_node.abspath()] = filter_name

					# Build list of uber files to files
					if generate_uber_file:
						uber_file_abspath = uber_file_node.abspath()
						if not uber_file_abspath in uber_file_to_file_list:
							uber_file_to_file_list[uber_file_abspath]  	= []
						uber_file_to_file_list[uber_file_abspath] 		+= [ file_node ]
			
			# Remember which sources come from which file list (for later lookup)
			file_list_to_source[file_list_file] = source_files + qt_source_files
			
	# Compute final source list based on platform	
	if platform == 'project_generator' or ctx.options.file_filter != "":
		# Collect all files plus uber files for project generators and when doing a single file compilation
		kw['source'] = uber_file_relative_list + source_files + qt_source_files + darwin_source_files + java_source_files + header_files + resource_files + other_files
		if platform == 'project_generator' and pch_file != '':
			kw['source'] += [ pch_file ] # Also collect PCH for project generators

	elif platform == 'cppcheck':
		# Collect source files and headers for cpp check
		kw['source'] = source_files + header_files
		
	else:
		# Regular compilation path
		if ctx.is_option_true('use_uber_files'):
			# Only take uber files when uber files are enabled and files not using uber files
			kw['source'] = uber_file_relative_list + no_uber_file_files + qt_source_files
		else:
			# Fall back to pure list of source files
			kw['source'] = source_files + qt_source_files
			
		# Append platform specific files
		platform_list = ctx.get_platform_list( platform )
		if 'darwin' in platform_list:
			kw['source'] += darwin_source_files
		if 'win' in platform_list:
			kw['source'] += resource_files
		kw['java_source'] = java_source_files
		
	# Handle PCH files
	if pch_file != '' and found_pch == False:
		# PCH specified but not found
		ctx.cry_file_error('[%s] Could not find PCH file "%s" in provided file list (%s).\nPlease verify that the name of the pch is the same as provided in a WAF file and that the PCH is not stored in an UberFile.' % (kw['target'], pch_file, ', '.join(file_lists)), 'wscript' )
	
	# Try some heuristic when to use PCH files
	if ctx.is_option_true('use_uber_files') and found_pch and len(uber_file_relative_list) > 0 and ctx.options.file_filter == "" and ctx.cmd != 'generate_uber_files':
		# Disable PCH files when having UberFiles as they  bring the same benefit in this case
		kw['pch_name'] = kw['pch']
		del kw['pch']
		
	# Store global lists in context	
	kw['task_generator_files'] 	= task_generator_files
	kw['file_list_content'] 		= file_list_content
	kw['project_filter'] 				=	file_to_project_filter
	kw['uber_file_lookup'] 			= uber_file_to_file_list
	kw['file_list_to_source']		= file_list_to_source
	
	
def VerifyInput(ctx, kw):
	"""
	Helper function to verify passed input values
	"""
	wscript_file = ctx.path.make_node('wscript').abspath()
	if kw['file_list'] == []:
		ctx.cry_file_error('TaskGenerator "%s" is missing mandatory parameter "file_list"' % kw['target'], wscript_file )
		
	if 'source' in kw:
		ctx.cry_file_error('TaskGenerator "%s" is using unsupported parameter "source", please use "file_list"' % kw['target'], wscript_file )

def InitializeTaskGenerator(ctx, kw):
	""" 
	Helper function to call all initialization routines requiered for a task generator
	"""		
	RegisterCryModule(ctx, kw)
	SanitizeInput(ctx, kw)
	VerifyInput(ctx, kw)
	AssignTaskGeneratorIdx(ctx, kw)
	RegisterVisualStudioFilter(ctx, kw)	
	TrackFileListChanges(ctx, kw)
	
target_modules = {}
def RegisterCryModule(ctx, kw):
	if 'target' not in kw:
		Logs.warn('Module in %s does not specify a target' % ctx.path.abspath())
		return
	target_modules[kw['target']] = kw
	
def ConfigureModuleUsers(ctx, kw):
	"""
	Helper function to maintain a list of modules use by this task generator
	"""
	if not hasattr(ctx, 'cry_module_users'):
		ctx.cry_module_users = {}
	
	for lib in kw['use_module']:		
		if not lib  in ctx.cry_module_users:
			ctx.cry_module_users[lib] = []
		ctx.cry_module_users[lib] += [ kw['target'] ]	
			
	
def LoadAddionalFileSettings(ctx, kw):
	"""
	Load all settings from the addional_settings parameter, and store them in a lookup map
	"""
	kw['features'] 							+= [ 'apply_additional_settings' ]	
	kw['file_specifc_settings'] = {}
	
	for setting in kw['additional_settings']:
				
		setting['target'] = kw['target'] # reuse target name
								
		file_list = []
		
		if 'file_list' in setting:	
			# Option A: The files are specifed as a *.waf_files (which is loaded already)
			for list in setting['file_list']:
				file_list += kw['file_list_to_source'][list]
			
		if 'files' in setting:		
			# Option B: The files are already specified as an list
			file_list += setting['files']			
			
		if 'regex' in setting:
			# Option C: A regex is specifed to match the files			
			p = re.compile(setting['regex'])

			for file in kw['source']: 
				if p.match(file):
					file_list += [file]
					

		# insert files into lookup dictonary, but make sure no uber file and no file within an uber file is specified
		uber_file_folder = ctx.bldnode.make_node('..')
		uber_file_folder = uber_file_folder.make_node('uber_files')
		uber_file_folder = uber_file_folder.make_node(kw['target'])
			
		for file in file_list:
			file_abspath 			= ctx.path.make_node(file).abspath()			
			uber_file_abspath = uber_file_folder.make_node(file).abspath()

			if 'uber_file_lookup' in kw:
				for uber_file in kw['uber_file_lookup']:

					# Uber files are not allowed for additional settings
					if uber_file_abspath == uber_file:
						ctx.cry_file_error("Addional File Settings are not support for UberFiles (%s) to ensure a consisten behaivor without UberFiles, please adjust your setup" % file, self.path.make_node('wscript').abspath())
						
					for entry in kw['uber_file_lookup'][uber_file]:						
						if file_abspath == entry.abspath():
							ctx.cry_file_error("Addional File Settings are not support for file using UberFiles (%s) to ensure a consisten behaivor without UberFiles, please adjust your setup" % file, self.path.make_node('wscript').abspath())
							
			# All fine, add file name to dictonary
			kw['file_specifc_settings'][file_abspath] = setting
			setting['source'] = []

@conf
def PostprocessBuildModules(ctx, *k, **kw):
	if hasattr(ctx, '_processed_modules'):
		return
	ctx._processed_modules = True
	platform = ctx.env['PLATFORM']
	for (u, kw) in target_modules.items():
		target = kw['target']
		configuration = ctx.GetConfiguration(target)

		pending_additions = dict()
		for k in kw:
			if k.endswith('use_module'):
				for m in kw[k]:
					prefix = k[:-len('use_module')]
					try:
						provides = target_modules[m]['module_provides']
						for k2 in provides:
							pending_additions.setdefault(prefix + k2, [])
							pending_additions[prefix + k2] += provides[k2]
					except KeyError as e:
						Logs.warn('Error applying module %s to module user %s' % (m, u))
						raise e
		for k in pending_additions:
			kw.setdefault(k, [])
			kw[k] += pending_additions[k]
		CollectPlatformSpecificSettings(ctx, kw, platform, configuration)
	
@conf
def ApplyPlatformSpecificSettings(ctx, kw, platform, configuration, spec):
	# First apply spec specific platform entries
	
	if platform == 'project_generator' and not ctx.cmd == 'generate_uber_files': # Get info when looping over all projects and configuration 
		return
	
	if not ctx.cmd == 'generate_uber_files': # generate_uber_files files does not know the concept of specs
		ApplySpecSpecificSettings(ctx, kw, platform, configuration, spec)	
	
	ApplyPlatformSpecificModuleExtension(ctx, kw, platform, configuration) # apply prior to ApplyPlatformSpecificModuleExtension() as it modifies the entry list on a per platform basis
	CollectPlatformSpecificSettings(ctx, kw, platform, configuration)	# will flatten all platform specific entries into a single entry (except when in project_generation mode)
	
def ConfigureTaskGenerator(ctx, kw):
	"""
	Helper function to apply default configurations and to set platform/configuration dependent settings
	"""	
	target = kw['target']
	spec = ctx.options.project_spec
	platform = ctx.env['PLATFORM']
	configuration = ctx.GetConfiguration(target)
	
	# Apply all settings, based on current platform and configuration
	ApplyConfigOverwrite(ctx, kw)	
	ctx.ApplyPlatformSpecificSettings(kw, platform, configuration, spec)	
	ApplyBuildOptionSettings(ctx, kw)
	
	# Load all file lists (including additional settings)
	file_list = kw['file_list']
	for setting in kw['additional_settings']:
		file_list += setting.get('file_list', [])
		
	LoadFileLists(ctx, kw, file_list)
	LoadAddionalFileSettings(ctx, kw)

	# Configure the modules users for static libraries
	ConfigureModuleUsers(ctx,kw)
		
	# Handle meta includes for WinRT
	for meta_include in kw.get('meta_includes', []):
		kw['cxxflags'] += [ '/AI' + meta_include ]		
	
	# Handle export definitions file	
	kw['linkflags'] 	+= [ '/DEF:' + ctx.path.make_node( export_file ).abspath() for export_file in kw['export_definitions']]
	
	# Generate output file name (without file ending), use target as an default if nothing is specified
	if kw['output_file_name'] == []:
		kw['output_file_name'] = target
	elif isinstance(kw['output_file_name'],list):
		kw['output_file_name'] = kw['output_file_name'][0] # Change list into a single string

	# Handle force_disable_mfc (Small Hack for Perforce Plugin (no MFC, needs to be better defined))
	if kw.get('force_disable_mfc', False) and '_AFXDLL' in kw['defines']:
		kw['defines'].remove('_AFXDLL')


def MonolithicBuildModule(ctx, *k, **kw):
	""" 
	Util function to collect all libs and linker settings for monolithic builds
	(Which apply all of those only to the final link as no DLLs or libs are produced)
	"""
	# Set up member for monolithic build settings
	if not hasattr(ctx, 'monolitic_build_settings'):
		ctx.monolitic_build_settings = {}
		
	# For game specific modules, store with a game unique prefix
	prefix = ''
	if kw.get('game_project', False):		
		prefix = kw['game_project'] + '_'

	# Collect libs for later linking
	def _append(key, values):
		if not ctx.monolitic_build_settings.get(key):
			ctx.monolitic_build_settings[key] = []
		ctx.monolitic_build_settings[key] += values
		
	_append(prefix + 'use', 			[ kw['target'] ] )
	_append(prefix + 'lib', 				kw['lib'] )
	_append(prefix + 'libpath', 		kw['libpath'] )
	_append(prefix + 'linkflags', 	kw['linkflags'] )

	
	# Adjust own task gen settings
	kw['defines'] += [ '_LIB' ]	
	
	# Remove rc files from the soures for monolithic builds (only the rc of the launcher will be used)
	tmp_src = []		
	for file in kw['source']:		
		if not file.endswith('.rc'):
			tmp_src += [file]
	kw['source'] = tmp_src		
		
	return ctx.objects(*k, **kw)
		
###############################################################################
def BuildTaskGenerator(ctx, kw):
	"""
	Check if this task generator should be build at all in the current configuration
	"""
	target 	= kw['target']
	
	
	if ctx.cmd == 'configure':
		return False 		# Dont build during configure
		
	if ctx.cmd == 'generate_uber_files':
		ctx(features='generate_uber_file', uber_file_list=kw['file_list_content'], target=target, pch=os.path.basename( kw.get('pch', '') ))
		return False 		# Dont do the normal build when generating uber files
		
	if ctx.env['PLATFORM'] == 'cppcheck':
		ctx(features='generate_uber_file', to_check_sources = kw['source_files'] + kw['header_files'], target=target)
		return False		# Dont do the normal build when running cpp check
		
	if ctx.env['PLATFORM'] == 'project_generator':
		return True			# Always include all projects when generating project for IDEs
			
	if target in ctx.spec_modules():
		return True		# Skip project is it is not part of the currecnt spec
	
	return False
	

		
@feature('apply_additional_settings')
@before_method('extract_vcxproj_overrides')
def tg_apply_additional_settings(self):
	""" 
	Apply all settings found in the addional_settings parameter after all compile tasks are generated 
	"""
	if len(self.file_specifc_settings) == 0:		
		return # no file specifc settings found

	for t in getattr(self, 'compiled_tasks', []):
		input_file = t.inputs[0].abspath()

		file_specific_settings = self.file_specifc_settings.get(input_file, None)
		
		if not file_specific_settings:
			continue
			
		t.env['CFLAGS'] 	+= file_specific_settings.get('cflags', [])
		t.env['CXXFLAGS'] += file_specific_settings.get('cxxflags', [])
		t.env['DEFINES'] 	+= file_specific_settings.get('defines', [])
		
		for inc in file_specific_settings.get('defines', []):
			if os.path.isabs(inc):
				t.env['INCPATHS'] += [ inc ]
			else:
				t.env['INCPATHS'] += [ self.path.make_node(inc).abspath() ]
	
###############################################################################
def set_cryengine_flags(ctx, kw):

	kw['includes'] = [ 
		'.', 
		ctx.CreateRootRelativePath('Code/SDKs/boost'), 
		ctx.CreateRootRelativePath('Code/SDKs/yasli'), 
		ctx.CreateRootRelativePath('Code/Libs/yasli'), 
		ctx.CreateRootRelativePath('Code/CryEngine/CryCommon') 
		] + kw['includes']
		
###############################################################################
@conf
def CryEngineModule(ctx, *k, **kw):
	"""
	CryEngine Modules are mostly compiled as dynamic libraries
	Except the build configuration requieres a monolithic build
	"""
	# Initialize the Task Generator
	InitializeTaskGenerator(ctx, kw)	

	# Setup TaskGenerator specific settings	
	set_cryengine_flags(ctx, kw)
	SetupRunTimeLibraries(ctx,kw)	
	if hasattr(ctx, 'game_project'):
		kw['game_project'] = ctx.game_project	
		
	ConfigureTaskGenerator(ctx, kw)

	if not BuildTaskGenerator(ctx, kw):
		return

	if _is_monolithic_build(ctx) and kw['target'] != 'AndroidLauncher':									# For monolithc builds, simply collect all build settings
		MonolithicBuildModule(ctx, getattr(ctx, 'game_project', None), *k, **kw)
		return
	
	# Temp monolothinc build hack for android
	if 'android' in ctx.env['PLATFORM']:
		kw['defines'] += [ '_LIB' ]
		kw['features'] += [ 'apply_monolithic_build_settings' ]
		active_project_name = ctx.spec_game_projects(ctx.game_project)
		
		if len (active_project_name) != 1:
			Logs.warn("Multiple project names found: %s, but only one is supported for android - using '%s'." % (active_project_name, active_project_name[0]))
			active_project_name = active_project_name[0]
			
		kw['project_name'] = "".join(active_project_name)
	
	kw['features'] 			+= [ 'generate_rc_file' ]		# Always Generate RC files for Engine DLLs
	if ctx.env['PLATFORM'] == 'darwin_x64':
		kw['mac_bundle'] 		= True										# Always create a Mac Bundle on darwin	

	ctx.shlib(*k, **kw)
	
def CreateStaticModule(ctx, *k, **kw):
	if ctx.cmd == 'configure':
		return
	
	if ctx.cmd == 'generate_uber_files':
		ctx(features='generate_uber_file', uber_file_list=kw['file_list_content'], target=kw['target'], pch=os.path.basename( kw.get('pch', '') ))
		return
		
	# Setup TaskGenerator specific settings	
	kw['kw'] 								= kw 
	kw['base_name'] 				= kw['target']
	kw['original_features'] = kw['features']
	kw['features'] 					= ['create_static_library']
	
	# And ensure that we run before all other tasks
	ctx.set_group('generate_static_lib')	
	ctx(*k, **kw)
	
	# Set back to default grp
	ctx.set_group('regular_group')
	
###############################################################################
@conf
def CryEngineStaticModule(ctx, *k, **kw):
	"""
	CryEngine Static Modules are compiled as static libraries
	Except the build configuration requieres a monolithic build
	"""	
	# Initialize the Task Generator
	InitializeTaskGenerator(ctx, kw)
	set_cryengine_flags(ctx, kw)

	ConfigureTaskGenerator(ctx, kw)
		
	CreateStaticModule(ctx, *k, **kw)

	
###############################################################################
@feature('create_static_library')
@before_method('apply_monolithic_build_settings')
@before_method('process_source')
def tg_create_static_library(self):
	"""
	This function resolves the 'use_module' directives, and transforms them to basic WAF 'use' directives.
	Unlike a normal 'use', this function will use certain settings from the consuming projects that will be "inherited".
	Thus, it's possible to have one module be instantiated for two distinct projects (ie, different CRT settings or different APIs).
	An example here is that certain DCC tools and RC will link with /MT instead of /MD, which would otherwise conflict.
	The "inherited" settings are defined in a white-list.
	"""
	if self.bld.env['PLATFORM'] == 'project_generator':
		return

	# Utility to perform whitelist filtering
	# Returns the intersection of the two lists
	def _filter(unfiltered, allowed):
		result = []
		for item in unfiltered:
			if item in allowed:
				result += [ item ]
		return result

	# Walks the use_module tree and collects the leaf nodes
	# This finds the WAF projects that (directly or indirectly) use the project 'node', and adds them to the 'out' list
	def _recursive_collect_leaves(bld, node, out, depth):
		node_users = bld.cry_module_users.get(node, [])
		if depth > 100:
			fatal('Circular dependency introduced, including at least module' + self.target)
		if len(node_users) == 0:
			if not node in out:
				out += [ node ]
		else:
			for user in node_users:
				_recursive_collect_leaves(bld, user, out, depth + 1)

	# 1. Pass: Find the candidate projects to which this module should be applied
	# For each module that uses this module, a set of properties that is inherited from the consuming project is computed
	# Each distinct set of properties will result in one "instantiation" of the module
	module_list = []
	_recursive_collect_leaves(self.bld, self.target, module_list, 1)

	tg_user_settings = []
	for module in module_list:
		tg_settings = {}

		if not module in self.bld.spec_modules():
			continue # Skip if the user is not configurated to be build in current spec
			
		# Get other task generator
		other_tg = self.bld.get_tgen_by_name(module)		
		
		# We will only use a subset of the settings of a user task generator
		for entry in 'defines cxxflags cflags'.split():
			if not entry in tg_settings:
				tg_settings[entry] = []

			# Filter the settings from the consumer of this module using the white-list
			filter_list = getattr(self.env, 'module_whitelist_' + entry, [])
			unfiltered_values = getattr(other_tg, entry, [])
			filtered_values = _filter(unfiltered_values, filter_list)

			tg_settings[entry] += filtered_values
		
		# Try to find a match for those settings
		bAddNewEntry = True
		for entry in tg_user_settings:	
			
			def _compare_settings():
				""" Peform a deep compare to ensure we have the same settings """
				for field in 'defines cxxflags cflags'.split():
					listA = entry['settings'][field]
					listB = tg_settings[field]
					if len(listA) != len(listB):
						return False
					listA.sort()
					listB.sort()
					for i in range(len(listA)):
						if listA[i] != listB[i]:
							return False
				return True
						
					
			if _compare_settings():
				entry['users'] += [ other_tg.name ] # Found an entry, just add ourself to users list
				bAddNewEntry = False
				break
				
		if bAddNewEntry == True:
			new_entry = {}
			new_entry['users'] = [ other_tg.name ]
			new_entry['settings'] = tg_settings
			tg_user_settings.append( new_entry )

	# 2. Pass for each user set, compute a MD5 to identify it and create new task generators
	for entry in tg_user_settings:
		
		def _compute_settings_md5(settings):
			""" Util function to compute the MD4 of all settings for a tg """
			keys = list(settings.keys())
			keys.sort()
			tmp = ''
			for k in keys:
				values = settings[k]
				values.sort()
				tmp += str([k, values])			
			return Utils.md5(tmp.encode()).hexdigest().upper()
		
		settings_md5 = _compute_settings_md5(entry['settings'])
		
		target = self.target + '.' + settings_md5
		if not 'clean_' in self.bld.cmd: # Dont output info when doing a clean build
			Logs.info("[WAF] Creating Static Library '%s' for module: '%s', SettingsHash: '%s'" % (self.target, ', '.join(entry['users']), settings_md5)	)
		
		
		kw = {}
		# Deep copy all source values
		for (key,value) in self.kw.items():
			if key == 'env':
				kw['env'] = value.derive()
			else:
				kw[key] = copy.copy(value)				
			
		# Append relevant users settings to ensure a consistent behavior (runtime CRT, potentially global defines)
		for (key, value) in entry['settings'].items():
			kw[key] += value
			
		# Patch fields which need unique values:
		kw['target'] = target
		kw['output_file_name'] = target		
		kw['idx'] = int(settings_md5[:8],16)	# Use part of the  MD5 as the per taskgenerator filename to ensure resueable object files
		kw['_type'] = 'stlib'
		kw['features'] = kw['original_features']
		
		# Tell the users to link this static library
		for user in entry['users']:
			user_tg = self.bld.get_tgen_by_name(user)
			user_tg.use += [ target ]
		
		# Ensure that we are executed
		if self.bld.targets != '' and not target in self.bld.targets:
			self.bld.targets += ',' + target
		
		# Enforce task generation for new tg		
		if _is_monolithic_build(self.bld):
			tg = MonolithicBuildModule(self.bld, **kw) #TODO this must get the settings from ther user in regards of game projects
		else:
			kw['features'] += ['c', 'cxx', 'cstlib', 'cxxstlib' ]			
			tg = self.bld(**kw)
			
		tg.path = self.path		# Patch Path
		tg.post()

	self.source = [] # Remove all source entries to prevent building the source task generator
	
###############################################################################
@conf
def CryLauncher(ctx, *k, **kw):
	"""
	Wrapper for CryEngine Executables
	"""
	# Initialize the Task Generator
	InitializeTaskGenerator(ctx, kw)	
				
	# Setup TaskGenerator specific settings	
	set_cryengine_flags(ctx, kw)
	SetupRunTimeLibraries(ctx,kw)	
	
	ConfigureTaskGenerator(ctx, kw)
		
	if not BuildTaskGenerator(ctx, kw):
		return
				
	# Copy kw dict and some internal values to prevent overwriting settings in one launcher from another
	kw_per_launcher = dict(kw)
	
	# Create multiple projects for each Launcher, based on the number of active projects in the current spec
	if ctx.env['PLATFORM'] == 'project_generator': 	# For the project generator, just use the first project (doesnt matter which project)
		active_projects = [ ctx.game_projects()[0] ]
	else: 																						# Else only use projects for current spec
		active_projects = ctx.spec_game_projects()
		
	for project in active_projects:
		kw_per_launcher['idx'] 				= kw['idx'] + (1000 * (ctx.project_idx(project) + 1));		
				
		kw_per_launcher['use'] 				= list(kw['use'])
		kw_per_launcher['lib'] 				= list(kw['lib'])
		kw_per_launcher['libpath'] 		= list(kw['libpath'])
		kw_per_launcher['linkflags'] 	= list(kw['linkflags'])
		
		# Setup values for Launcher Projects
		kw_per_launcher['features'] 				+= [ 'generate_rc_file' ]	
		kw_per_launcher['is_launcher'] 			= True
		kw_per_launcher['resource_path'] 		= ctx.launch_node().make_node(ctx.game_code_folder(project) + '/Resources')
		kw_per_launcher['project_name'] 		= project
		kw_per_launcher['output_file_name'] 	= ctx.get_executable_name( project )
		
		if _is_monolithic_build(ctx):	
			kw_per_launcher['defines'] += [ '_LIB' ]
			kw_per_launcher['features'] += [ 'apply_monolithic_build_settings' ]
					
		ctx.program(*k, **kw_per_launcher)	
	
	
###############################################################################
@conf
def CryDedicatedServer(ctx, *k, **kw):
	"""	
	Wrapper for CryEngine Dedicated Servers
	"""
	# Initialize the Task Generator
	InitializeTaskGenerator(ctx, kw)	

	# Setup TaskGenerator specific settings	
	set_cryengine_flags(ctx, kw)
	SetupRunTimeLibraries(ctx,kw)
	kw.setdefault('win_linkflags', []).extend(['/SUBSYSTEM:WINDOWS'])
	
	ConfigureTaskGenerator(ctx, kw)
		
	if not BuildTaskGenerator(ctx, kw):
		return
					
	# Copy kw dict and some internal values to prevent overwriting settings in one launcher from another
	kw_per_launcher = dict(kw)
	
	# Create multiple projects for each Launcher, based on the number of active projects in the current spec
	if ctx.env['PLATFORM'] == 'project_generator': 	# For the project generator, just use the first project (doesnt matter which project)
		active_projects = [ ctx.game_projects()[0] ]
	else: 																						# Else only use projects for current spec
		active_projects = ctx.spec_game_projects()
		
	for project in active_projects:
	
		kw_per_launcher['idx'] 				= kw['idx'] + (1000 * (ctx.project_idx(project) + 1));		
		
		kw_per_launcher['use'] 				= list(kw['use'])
		kw_per_launcher['lib'] 				= list(kw['lib'])
		kw_per_launcher['libpath'] 		= list(kw['libpath'])
		kw_per_launcher['linkflags'] 	= list(kw['linkflags'])
		
		kw_per_launcher['features'] 						+= [ 'generate_rc_file' ]
		kw_per_launcher['is_dedicated_server']	 = True
		kw_per_launcher['resource_path'] 				= ctx.launch_node().make_node(ctx.game_code_folder(project) + '/Resources')
		kw_per_launcher['project_name'] 				= project
		kw_per_launcher['output_file_name'] 			= ctx.get_dedicated_server_executable_name( project )
		
		if _is_monolithic_build(ctx):
			kw_per_launcher['defines'] += [ '_LIB' ]
			kw_per_launcher['features'] += [ 'apply_monolithic_build_settings' ]

		ctx.program(*k, **kw_per_launcher)
	
###############################################################################
@conf
def CryConsoleApplication(ctx, *k, **kw):
	"""
	Wrapper for CryEngine Executables
	"""
	# Initialize the Task Generator
	InitializeTaskGenerator(ctx, kw)	

	# Setup TaskGenerator specific settings	
	set_cryengine_flags(ctx, kw)
	SetupRunTimeLibraries(ctx,kw)
	kw.setdefault('win_linkflags', []).extend(['/SUBSYSTEM:CONSOLE'])
	
	ConfigureTaskGenerator(ctx, kw)
		
	if not BuildTaskGenerator(ctx, kw):
		return
		
	ctx.program(*k, **kw)
	
###############################################################################
@conf
def CryFileContainer(ctx, *k, **kw):	
	"""
	Function to create a header only library
	"""
	# Initialize the Task Generator
	InitializeTaskGenerator(ctx, kw)	

	# Setup TaskGenerator specific settings
	ConfigureTaskGenerator(ctx, kw)
		
	if not BuildTaskGenerator(ctx, kw):
		return
        
	if ctx.env['PLATFORM'] == 'project_generator':
		ctx(*k, **kw)	
		
		
###############################################################################
@conf
def CryEditor(ctx, *k, **kw):
	"""
	Wrapper for CryEngine Editor Executables
	"""
	# Initialize the Task Generator
	InitializeTaskGenerator(ctx, kw)	

	# Setup TaskGenerator specific settings	
	ctx.set_editor_flags(kw)
	SetupRunTimeLibraries(ctx,kw)
	kw['features'] 			+= [ 'generate_rc_file' ]
	kw['defines'] 			+= [ 'SANDBOX_EXPORTS' ]
	kw.setdefault('win_linkflags', []).extend(['/SUBSYSTEM:WINDOWS'])
	
	ConfigureTaskGenerator(ctx, kw)
		
	if not BuildTaskGenerator(ctx, kw):
		return
		
	ctx.program(*k, **kw)
	
###############################################################################
@conf
def CryPlugin(ctx, *k, **kw):
	"""
	Wrapper for CryEngine Editor Plugins
	"""
	# Initialize the Task Generator
	InitializeTaskGenerator(ctx, kw)	

	# Setup TaskGenerator specific settings	
	ctx.set_editor_flags(kw)
	SetupRunTimeLibraries(ctx,kw)
	kw['cxxflags'] 						+= ['/EHsc', '/wd4251', '/wd4275']	
	kw['cflags'] 						  += ['/EHsc']
	kw['defines'] 						+= [ 'SANDBOX_IMPORTS', 'PLUGIN_EXPORTS', 'EDITOR_COMMON_IMPORTS', 'NOT_USE_CRY_MEMORY_MANAGER' ]
	kw['output_sub_folder'] 	= 'EditorPlugins'
	
	ConfigureTaskGenerator(ctx, kw)
		
	if not BuildTaskGenerator(ctx, kw):
		return
		
	ctx.shlib(*k, **kw)
	
###############################################################################
@conf
def CryPluginModule(ctx, *k, **kw):
	"""
	Wrapper for CryEngine Editor Plugins Util dlls, those used by multiple plugins
	"""
	# Initialize the Task Generator
	InitializeTaskGenerator(ctx, kw)	

	# Setup TaskGenerator specific settings	
	ctx.set_editor_flags(kw)
	SetupRunTimeLibraries(ctx,kw)
	kw['cxxflags'] 	+= ['/EHsc', '/wd4251', '/wd4275']
	kw['cflags'] 		+= ['/EHsc']
	kw['defines'] 	+= [ 'PLUGIN_EXPORTS', 'EDITOR_COMMON_EXPORTS', 'NOT_USE_CRY_MEMORY_MANAGER' ]
	
	ConfigureTaskGenerator(ctx, kw)
		
	if not BuildTaskGenerator(ctx, kw):
		return
		
	ctx.shlib(*k, **kw)
	

###############################################################################
@conf
def CryResourceCompiler(ctx, *k, **kw):
	"""
	Wrapper for RC
	"""
	# Initialize the Task Generator
	InitializeTaskGenerator(ctx, kw)	

	# Setup TaskGenerator specific settings	
	SetupRunTimeLibraries(ctx,kw, 'static')
	
	ctx.set_rc_flags(kw)	
	kw['output_file_name'] 	= 'rc'
	kw['output_sub_folder'] = ctx.CreateRootRelativePath('Tools/rc')
	
	kw.setdefault('win_linkflags', []).extend(['/SUBSYSTEM:CONSOLE'])
	kw.setdefault('debug_linkflags', []).extend(['/NODEFAULTLIB:libcmt.lib', '/NODEFAULTLIB:msvcprtd.lib'])
	
	ConfigureTaskGenerator(ctx, kw)

	if not BuildTaskGenerator(ctx, kw):
		return

	ctx.program(*k, **kw)

###############################################################################
@conf
def CryResourceCompilerModule(ctx, *k, **kw):
	"""
	Wrapper for RC modules
	"""
	# Initialize the Task Generator
	InitializeTaskGenerator(ctx, kw)	

	# Setup TaskGenerator specific settings	
	SetupRunTimeLibraries(ctx,kw, 'static')
	ctx.set_rc_flags(kw)
	kw['output_sub_folder'] = ctx.CreateRootRelativePath('Tools/rc')
	kw.setdefault('win_linkflags', []).extend(['/SUBSYSTEM:CONSOLE'])
	kw.setdefault('debug_linkflags', []).extend(['/NODEFAULTLIB:libcmt.lib', '/NODEFAULTLIB:msvcprtd.lib'])
	
	ConfigureTaskGenerator(ctx, kw)

	if not BuildTaskGenerator(ctx, kw):
		return

	ctx.shlib(*k, **kw)

###############################################################################
@conf
def CryPipelineModule(ctx, *k, **kw):
	"""
	Wrapper for Pipleine modules (mostly DCC exporters)
	"""
	# Initialize the Task Generator
	InitializeTaskGenerator(ctx, kw)	

	# Setup TaskGenerator specific settings	
	SetupRunTimeLibraries(ctx,kw, 'static')
	ctx.set_pipeline_flags(kw)
	kw.setdefault('win_linkflags', []).extend(['/SUBSYSTEM:CONSOLE'])
	kw.setdefault('debug_linkflags', []).extend(['/NODEFAULTLIB:libcmt.lib', '/NODEFAULTLIB:msvcprtd.lib'])
	
	ConfigureTaskGenerator(ctx, kw)
		
	if not BuildTaskGenerator(ctx, kw):
		return

	ctx.shlib(*k, **kw)

###############################################################################
# Helper function to set Flags based on options
def ApplyBuildOptionSettings(self, kw):
	"""
	Util function to apply flags based on waf options
	"""		
	# Add debug flags if requested
	if self.is_option_true('generate_debug_info'):
		kw['cflags'].extend(self.env['COMPILER_FLAGS_DebugSymbols'])
		kw['cxxflags'].extend(self.env['COMPILER_FLAGS_DebugSymbols'])
		kw['linkflags'].extend(self.env['LINKFLAGS_DebugSymbols'])

	# Add show include flags if requested	
	if self.is_option_true('show_includes'):		
		kw['cflags'].extend(self.env['SHOWINCLUDES_cflags'])
		kw['cxxflags'].extend(self.env['SHOWINCLUDES_cxxflags'])
	
	# Add preprocess to file flags if requested	
	if self.is_option_true('show_preprocessed_file'):	
		kw['cflags'].extend(self.env['PREPROCESS_cflags'])
		kw['cxxflags'].extend(self.env['PREPROCESS_cxxflags'])	
		self.env['CC_TGT_F'] = self.env['PREPROCESS_cc_tgt_f']
		self.env['CXX_TGT_F'] = self.env['PREPROCESS_cxx_tgt_f']	
	
	# Add disassemble to file flags if requested	
	if self.is_option_true('show_disassembly'):	
		kw['cflags'].extend(self.env['DISASSEMBLY_cflags'])
		kw['cxxflags'].extend(self.env['DISASSEMBLY_cxxflags'])	
		self.env['CC_TGT_F'] = self.env['DISASSEMBLY_cc_tgt_f']
		self.env['CXX_TGT_F'] = self.env['DISASSEMBLY_cxx_tgt_f']
		
	
###############################################################################
# Helper function to extract platform specific flags
@conf
def GetPlatformSpecificSettings(ctx, kw, entry, _platform, _configuration):
	"""
	Util function to apply flags based on current platform
	"""
	def _to_list( value ):
		if isinstance(value,list):
			return value
		return [ value ]
	
	def _flatten(coll):
		for i in coll:
			if isinstance(i, Iterable) and not isinstance(i, basestring):
				for subc in _flatten(i):
					yield subc
			else:
				yield i
				
	returnValue = {}	
	platforms 	= ctx.get_platform_list( _platform )

	# If not configuration defined load all configurations
	if not _configuration:
		configurations = ctx.get_supported_configurations()
	else:
		configurations = [_configuration]

	# Check for entry in <platform>_<entry> style
	for platform in platforms:
		platform_entry = platform + '_' + entry 
		if platform_entry in kw:
			if kw[platform_entry]:
				returnValue[platform_entry] = _to_list( kw[platform_entry] )
				
	# Check for entry in <configuration>_<entry> style
	for configuration in configurations:
		configuration_entry = configuration + '_' + entry
		if configuration_entry in kw:
			if kw[configuration_entry]:
				returnValue[configuration_entry] = _to_list( kw[configuration_entry] )
	
	# Check for entry in <platform>_<configuration>_<entry> style
	for platform in platforms:
		for configuration in configurations:
			platform_configuration_entry =   platform + '_' + configuration + '_' + entry
			if platform_configuration_entry in kw:
				if kw[platform_configuration_entry]:
					returnValue[platform_configuration_entry] = _to_list( kw[platform_configuration_entry] )			

	# flatten to a single entry
	if not platform == 'project_generator' or ctx.cmd == 'generate_uber_files':
		returnValue = list(itertools.chain.from_iterable(returnValue.values()))
		_flatten(returnValue)
		returnValue = _preserved_order_remove_duplicates(returnValue) # strip duplicate
		returnValue = {entry : returnValue } # convert to dict 
		
	return returnValue
	
###############################################################################
@conf
def get_platform_permutation_map(ctx, platform):
	platform_permutations 	= ctx.get_platform_list( platform )
	supported_platforms = ctx.get_supported_platforms()
	
	# Map all platform_permutations to supported platforms
	# 'win' -> ['win_x86', 'win_x64']
	palatform_conversion_map = dict()	
	for platform_mutation in platform_permutations:
		matching_platforms = [s for s in supported_platforms if platform_mutation in s]
		palatform_conversion_map[platform_mutation] = matching_platforms
		
	return palatform_conversion_map
	
###############################################################################
# Helper function to apply platform specific module extensions
def ApplyPlatformSpecificModuleExtension(ctx, kw, _platform, _configuration):
	"""
	Util function to apply module extensions to platform and configuration 
	"""
	# This function will only call ctx.execute_module_extension() with supported platforms name e.g. win_x86 NOT win
	# 1) if no permutation or configuration detected, ctx.execute_module_extension() is call with ALL valid supported platforms
	# 2) if a permutation is detected e.g. "win", ctx.execute_module_extension() is call for all matching supported platforms e.g. win_x86 AND win_x64
	# 3) if a configuration only type is detected e.g. "profile", ctx.execute_module_extension() is call with ALL valid supported platforms for this build e.g. win_x86_profile, win_x64_profile, linux_x86_profile,...
	# 4) if a permutation and a configuration type is detected, ctx.execute_module_extension() is called for that individual case e.g. win_x64_profile
	#
	# Note:
	# Case 1) and 2) may result in the same ctx.execute_module_extension(). Where 1) is valid for all platforms and 2) only for the special platform permutation case.
	# Case 3) and 4) may result in the same ctx.execute_module_extension(). Where 3) is valid for all platforms and 4) only for the special platform+console permutation case.
	# However 2) and 4) might have even further module_extensions that should only be executed for that platform compared to 1) and 3)
	#   [valid]   e.g. profile_module_extension = ['SomeExtension'] and win_x64_profile = ['OtherExtension'] and/or win_x64_profile = ['EvenOtherExtension']
	#   [invalid] e.g. profile_module_extension = ['SomeExtension'] and win_x64_profile = ['SomeExtension'] and/or win_x64_profile = ['SomeExtension','OtherExtension']
	
	def _to_list( value ):
		if isinstance(value,list):
			return value
		return [ value ]
		
	supported_platforms = ctx.get_supported_platforms()
	platform_permutations 	= ctx.get_platform_list( _platform )
	palatform_conversion_map = ctx.get_platform_permutation_map(_platform)
		
	# If not configuration or 'project_generator' load all configurations. (project_generator has to know about every potential permutation)
	if not _configuration or _configuration == 'project_generator':
		configurations = ctx.get_supported_configurations()
	else:
		configurations = [_configuration]

	# 1) Check for entry in <entry> style
	for module_extension in _to_list( kw['module_extensions'] ):
		for supported_platform in supported_platforms:
			ctx.execute_module_extension(module_extension, kw, '%s_' %(supported_platform), supported_platform, "")

	# 2) Check for entry in <platform_mutation>_<entry> style
	platform_entry_set = set()
	for platform_mutation in platform_permutations:
		platform_entry = platform_mutation + '_module_extensions'
		if platform_entry in kw:
			for module_extension in _to_list( kw[platform_entry] ):
				for supported_platform in palatform_conversion_map[platform_mutation]:
					ctx.execute_module_extension(module_extension, kw, '%s_' %(supported_platform), supported_platform, "")

	# 3) Check for entry in <configuration>_<entry> style	
	for configuration in configurations:
		configuration_entry = configuration + '_module_extensions'
		if configuration_entry in kw:
			for module_extension in _to_list( kw[configuration_entry] ):
				for supported_platform in supported_platforms:
					ctx.execute_module_extension(module_extension, kw, '%s_%s_' %(supported_platform, configuration), supported_platform, configuration)

	# 4) Check for entry in <platform_mutation>_<configuration>_<entry> style
	for platform_mutation in platform_permutations:
		for configuration in configurations:
			platform_configuration_entry =   platform_mutation + '_' + configuration + '_module_extensions'
			if platform_configuration_entry in kw:
				for module_extension in _to_list( kw[platform_configuration_entry] ):
					for supported_platform in palatform_conversion_map[platform_mutation]:
						ctx.execute_module_extension(module_extension, kw, '%s_%s_' %(supported_platform, configuration), supported_platform, configuration)						

###############################################################################
# Wrapper for ApplyPlatformSpecificFlags for all flags to apply
def CollectPlatformSpecificSettings(ctx, kw, platform, configuration):
	"""
	Check each compiler/linker flag for platform specific additions
	"""
	# handle list entries
	for entry in 'additional_settings use_module export_definitions meta_includes file_list use defines includes cxxflags cflags lib libpath linkflags framework features'.split():						
		for key, value in GetPlatformSpecificSettings(ctx, kw, entry, platform, configuration).iteritems():
			kw[key] += value

	# Handle string entries
	for entry in 'output_file_name'.split():
		for key, value in GetPlatformSpecificSettings(ctx, kw, entry, platform, configuration).iteritems():
			if kw[key] == []: # no general one set
				kw[key] = value

	# Recurse for additional settings
	for setting in kw['additional_settings']:
		CollectPlatformSpecificSettings(ctx, setting, platform, configuration)

###############################################################################
# Set env in case a env overwrite is specified for this project
def ApplyConfigOverwrite(ctx, kw):	
	
	target = kw['target']
	if not target in ctx.env['CONFIG_OVERWRITES']:
		return
		
	platform 					=  ctx.env['PLATFORM']
	overwrite_config 	= ctx.env['CONFIG_OVERWRITES'][target]
	
	# Need to set crytek specific shortcuts if loading another environment
	ctx.all_envs[platform + '_' + overwrite_config]['PLATFORM']  			= platform
	ctx.all_envs[platform + '_' + overwrite_config]['CONFIGURATION']  = overwrite_config
	
	# Create a deep copy of the env for overwritten task to prevent modifying other task generator envs
	kw['env'] = ctx.all_envs[platform + '_' + overwrite_config].derive()
	kw['env'].detach()
	
###############################################################################
def ApplySpecSpecificSettings(ctx, kw, platform, configuration, spec):
	
	if platform == 'project_generator' or platform == []:
		return {} 	# Return only an empty dict when generating a project
		
	# To DO:
	## handle list entries
	#for entry in 'use_module use export_definitions meta_includes file_list  defines includes cxxflags cflags lib libpath linkflags framework features'.split():		
	#	for key, value in GetPlatformSpecificSettings(ctx, kw, entry, platform, configuration).iteritems():
	#		kw[key] += value
	#		
	## Handle string entries
	#for entry in 'output_file_name'.split():
	#	for key, value in GetPlatformSpecificSettings(ctx, kw, entry, platform, configuration).iteritems():
	#		if kw[key] == []: # no general one set
	#			kw[key] = value

	for key, value in ctx.spec_defines(spec, platform, configuration).iteritems():
		kw[key] += value
		
	for key, value in  ctx.spec_module_extensions(spec, platform, configuration).iteritems():
		kw[key] += value
		
	for key, value in  ctx.spec_features(spec, platform, configuration).iteritems():
		kw[key] += value

###############################################################################		
def _is_monolithic_build(ctx):
	if ctx.env['PLATFORM'] == 'project_generator':
		return False

	return ctx.spec_monolithic_build()

@feature('apply_monolithic_build_settings')
@before_method('process_source')
def apply_monolithic_build_settings(self):
	# Add collected settings to link task	
	try:
		self.use  += self.bld.monolitic_build_settings['use']
		self.lib  += self.bld.monolitic_build_settings['lib']
		self.libpath    += self.bld.monolitic_build_settings['libpath']
		self.linkflags  += self.bld.monolitic_build_settings['linkflags']
		
		# Add game specific files
		prefix = self.project_name + '_'
		self.use  += self.bld.monolitic_build_settings[prefix + 'use']
		self.lib  += self.bld.monolitic_build_settings[prefix + 'lib']
		self.libpath    += self.bld.monolitic_build_settings[prefix + 'libpath']
		self.linkflags  += self.bld.monolitic_build_settings[prefix + 'linkflags']
		
		self.use  = _preserved_order_remove_duplicates(self.use)
		self.lib  = _preserved_order_remove_duplicates(self.lib)
		self.libpath   = _preserved_order_remove_duplicates(self.libpath)
		self.linkflags = _preserved_order_remove_duplicates(self.linkflags)
		
	except:
		Logs.fatal('[ERROR]:failed in apply monolithic')

@feature('copy_bin_output_to_all_platform_output_folders')
@after_method('set_pdb_flags')
@after_method('apply_incpaths')
@after_method('apply_map_file')
def feature_copy_bin_output_to_all_platform_output_folders(self):
	bld 			= self.bld
	platform	= bld.env['PLATFORM']
	configuration = bld.env['CONFIGURATION']
		
	if platform  == 'project_generator' or not bld.options.project_spec: # only support build that have a project spec
		return
	
	if not getattr(self, 'link_task', None):
		return	
		
	if self._type == 'stlib': # Do not copy static libs
		return

	# Copy output file into all configuration folders
	for additional_out_config in self.bld.get_supported_configurations():	
		if additional_out_config != configuration:
			output_folder = self.bld.get_output_folders(platform, additional_out_config)[0]		
			for src in self.link_task.outputs:				
				if not src.abspath().endswith('.manifest'):
					self.create_task('copy_outputs', src, output_folder)
					self.copy_files(src, [output_folder])
					
