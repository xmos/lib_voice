# Copyright 2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
"""Shared pytest collector for Unity-framework `.xe` test executables ("Pattern A").

Reused by conftest.py files that discover `.xe` binaries under a test directory and run them via
`xrun` (hardware, via xtagctl) or `xsim` (simulator, selected with the shared `--sim` flag) - so
each suite doesn't duplicate the same `pytest_collect_file`/`pytest.Item` boilerplate.

This module registers NO options of its own - it reads the shared `--arch`/`--sim` options
registered once by `tests/conftest.py` (see `arch_option.py`). A suite using this module must NOT
register its own `--arch`/`--sim` option, and must NOT keep its own local `pytest.ini` (that would
cut it off from the shared options - pytest only loads conftest.py files down to the nearest ini
file's directory).

Most suites collect with the "standard" Unity output dialect (the module-level `pytest_collect_file`
below). A suite whose Unity harness prints output in a different format can build its own
`pytest_collect_file` via `make_pytest_collect_file(dialect=...)` - currently only `aec_unit_tests`
needs this ("legacy_aec" dialect: matches `PASS`/`FAIL` directly instead of `test`, strips a stray
`C:` prefix from the line, and raises a single-arg exception with no dict/repr_failure detail).

A suite whose captured output never reliably emits a matching `PASS` line (e.g. `ns_unit_tests`'s
fixture-based multi-`RUN_TEST_GROUP` `.xe`, which is not built with the auto-test-runner) can pass
`make_pytest_collect_file(check_pass_found=False)` to skip the final "no PASS/FAIL line seen at
all" check, which would otherwise false-fail an actually-passing run.
"""
import pytest
import subprocess
import xtagctl
from arch_option import resolve_arches

HW_TARGETS = {
    "xs3a": "XCORE-AI-EXPLORER",
    "vx4b": "XK-EVK-XU416",
}

DEFAULT_ARCH = ("xs3a",)


def make_pytest_collect_file(dialect="standard", check_pass_found=True):
    """Build a `pytest_collect_file` hook that collects `.xe` files with the given output dialect."""
    def pytest_collect_file(parent, file_path):
        if file_path.suffix == ".xe":
            return UnityTestSource.from_parent(parent, path=file_path, dialect=dialect,
                                                check_pass_found=check_pass_found)
    return pytest_collect_file


pytest_collect_file = make_pytest_collect_file()


class UnityTestSource(pytest.File):
    def __init__(self, *args, dialect="standard", check_pass_found=True, **kwargs):
        super().__init__(*args, **kwargs)
        self.dialect = dialect
        self.check_pass_found = check_pass_found

    def collect(self):
        sim = self.config.getoption("sim")
        arches = resolve_arches(self.config, DEFAULT_ARCH)
        # Only disambiguate names when more than one arch/sim combination is selected, so the
        # common single-arch case keeps today's plain executable name (junit history/tooling).
        suffix_names = len(arches) > 1 or sim
        for arch in arches:
            name = f"{self.name}[{arch}{'-sim' if sim else ''}]" if suffix_names else self.name
            yield UnityTestExecutable.from_parent(
                self, fspath=self.fspath, name=name, arch=arch, sim=sim, dialect=self.dialect,
                check_pass_found=self.check_pass_found
            )


class UnityTestExecutable(pytest.Item):
    def __init__(self, fspath, name, parent, arch, sim=False, dialect="standard", check_pass_found=True):
        super(UnityTestExecutable, self).__init__(name, parent)
        self.fspath = fspath
        self.arch = arch
        self.sim = sim
        self.dialect = dialect
        self.check_pass_found = check_pass_found
        self._nodeid = name  # Override the naming to suit C better

    def runtest(self):
        # Run the binary on hardware, or in the simulator
        simulator_fail = False
        test_output = None
        try:
            print(f"run executable {self.fspath} on arch {self.arch}{' (sim)' if self.sim else ''}")
            if self.sim:
                test_output = subprocess.check_output(['xsim', self.fspath], text=True, stderr=subprocess.STDOUT)
            elif self.arch in HW_TARGETS:
                with xtagctl.acquire(HW_TARGETS[self.arch], timeout=3600) as adapter_id:
                    test_output = subprocess.check_output(['xrun', '--io', '--adapter-id', adapter_id, self.fspath], text=True, stderr=subprocess.STDOUT)
            else:
                assert 0, f"Architecture {self.arch} not supported"
        except subprocess.CalledProcessError as e:
            # Unity exits non-zero if an assertion fails
            simulator_fail = True
            test_output = e.output

        # Parse the Unity output
        unity_pass = False
        for line in test_output.split('\n'):
            if self.dialect == "legacy_aec":
                if not ('PASS' in line or 'FAIL' in line):
                    continue
                test_report = line.removeprefix('C:').split(':')
            else:
                if 'test' not in line:
                    continue
                test_report = line.split(':')
            # Unity output is as follows:
            #   <test_source>:<line_number>:<test_case>:PASS
            #   <test_source>:<line_number>:<test_case>:FAIL:<failure_reason>
            test_source, line_number, test_case, result = test_report[0], test_report[1], test_report[2], test_report[3]
            print(('\n {}()'.format(test_case)), end=' ')
            if result == 'PASS':
                unity_pass = True
                continue
            if result == 'FAIL':
                failure_reason = test_report[4]
                if self.dialect == "legacy_aec":
                    print('\n' + '\n'.join([str(self.parent).strip('<>'),
                                            '{}:{}:{}()'.format(test_source, line_number, test_case),
                                            'Failure reason:', failure_reason]))
                    raise UnityTestException(self)
                print('')  # Insert line break after test_case print
                raise UnityTestException(self, {'test_source': test_source,
                                                'line_number': line_number,
                                                'test_case': test_case,
                                                'failure_reason':
                                                    failure_reason})

        if simulator_fail:
            raise Exception(self, "Simulation failed.")
        if self.check_pass_found and not unity_pass:
            raise Exception(self, "Unity test output not found.")
        print('')  # Insert line break after final test_case which passed

    def repr_failure(self, excinfo):
        if isinstance(excinfo.value, UnityTestException) and len(excinfo.value.args) > 1:
            return '\n'.join([str(self.parent).strip('<>'),
                              '{}:{}:{}()'.format(
                                    excinfo.value.args[1]['test_source'],
                                    excinfo.value.args[1]['line_number'],
                                    excinfo.value.args[1]['test_case']),
                              'Failure reason:',
                              excinfo.value.args[1]['failure_reason']])
        else:
            return str(excinfo.value)

    def reportinfo(self):
        # It's not possible to give sensible line number info for an executable
        # so we return it as 0.
        #
        # The source line number will instead be recovered from the Unity print
        # statements.
        return self.fspath, 0, self.name


class UnityTestException(Exception):
    pass

