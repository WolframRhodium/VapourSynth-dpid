project('VapourSynth-dpid', 'cpp',
  default_options: ['buildtype=release', 'b_ndebug=if-release', 'cpp_std=c++17', 'warning_level=3'],
  meson_version: '>=0.49.0',
  version: '8'
)

cxx = meson.get_compiler('cpp')
if cxx.get_argument_syntax() == 'gcc' or cxx.get_id() == 'clang-cl'
  add_project_arguments('-Wno-unused-parameter', language: 'cpp')
endif

sources = [
  'Source.cpp',
]

if build_machine.system() == 'windows'
  deps = []
  install_dir = 'installed'  # dummy
else
  vapoursynth_deps = [dependency('vapoursynth').partial_dependency(compile_args: true, includes: true)]
  deps = [vapoursynth_deps]
  install_dir = join_paths(vapoursynth_dep.get_pkgconfig_variable('libdir'), 'vapoursynth')
endif

shared_module('dpid', sources,
  dependencies: deps,
  install: true,
  install_dir: install_dir,
  gnu_symbol_visibility: 'hidden'
)

