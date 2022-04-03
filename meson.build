project('bsl', 'c')

src = [
  'src/bsl.c',
  'src/lexer.c',
  'src/parser.c',
  'src/util.c',
  'src/resolve.c',
]

inc = include_directories('.')
priv_inc = include_directories('include')

bsl_lib = static_library('bsl',
                          src,
                          c_args : ['-DCWIN_BACKEND_WIN32'],
                          include_directories : [inc, priv_inc],
)

bsl_dep = declare_dependency(link_with : bsl_lib,
                              include_directories : inc,
)
