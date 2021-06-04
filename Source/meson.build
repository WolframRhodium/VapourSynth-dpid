project('VapourSynth-dpid', 'cpp',
  default_options: ['buildtype=release', 'b_ndebug=if-release', 'cpp_std=c++17', 'warning_level=3'],
  meson_version: '>=0.48.0',
  version: '8'
)

add_project_arguments('-Wno-unused-parameter', language: 'cpp')

sources = [
  'Source.cpp',
]

vapoursynth_dep = dependency('vapoursynth').partial_dependency(compile_args: true, includes: true)

libs = []

shared_module('dpid', sources,
  dependencies: vapoursynth_dep,
  link_with: libs,
  install: true,
  install_dir: join_paths(vapoursynth_dep.get_pkgconfig_variable('libdir'), 'vapoursynth'),
  gnu_symbol_visibility: 'hidden'
)
