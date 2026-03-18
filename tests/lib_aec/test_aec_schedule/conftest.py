
def pytest_addoption(parser):
  parser.addoption(
    "--arch",
    nargs = "+",
    default = ["xs3a"],
    help = "One or more architectures to run on (e.g. --arch xs3a sim)",
    choices = ["xs3a", "vx4b"],
  )

def pytest_generate_tests(metafunc):
  if "target" in metafunc.fixturenames:
    selected_arches = metafunc.config.getoption("arch")
    if isinstance(selected_arches, str):
      selected_arches = [selected_arches]
    metafunc.parametrize("target", selected_arches)
