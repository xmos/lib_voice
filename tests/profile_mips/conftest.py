# Copyright 2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
from pathlib import Path
import json
import re

def pytest_addoption(parser):
    parser.addoption(
        "--arch",
        nargs="+",
        default=["xs3a"],
        help="One or more architectures to run on (e.g. --arch xs3a vx4b)",
        choices=["xs3a", "vx4b"],
    )
    parser.addoption(
      "--update",
      action="store_true",
      help=("Regenerate `lib_voice_mips.json` and `lib_voice_mips_table.rst`. "
          "The comparison check which flags mips being out of range doesn't run in this case.")
    )

def write_rst_table(configs: dict, outfile: Path):
    """
    Generate a reStructuredText table summarizing MIPS usage per architecture.

    Parameters
    ----------
    configs : dict
        Nested dict mapping architecture name to a dict of app_name -> mips value.
        e.g. {"xs3a": {"app_mips_ns": 20.79}, "vx4b": {"app_mips_ns": 19.1}}
    outfile : pathlib.Path
        Output path for generated RST file.
    """
    archs = sorted(configs.keys())
    all_apps = sorted({app for apps in configs.values() for app in apps})
    widths = " ".join(["8"] * (1 + len(archs)))
    lines = [
        ".. _lib_voice_mips_usage:\n",
        ".. list-table:: CPU requirements (600 MHz system frequency, 120 MHz per HW thread)",
        "   :header-rows: 1",
        f"   :widths: {widths}",
        "",
        "   * - Component",
    ]
    for arch in archs:
        lines.append(f"     - MIPS use ({arch.upper()})")
    for app in all_apps:
        m = re.search(r"app_mips_([^\s]+)", app)
        assert m, "Cannot parse app name. Should start with app_mips_"
        app_name = m.group(1).upper()
        lines.append(f"   * - {app_name}")
        for arch in archs:
            mips = configs[arch].get(app, "N/A")
            lines.append(f"     - {mips}")
    outfile.write_text("\n".join(lines))

def pytest_sessionstart(session):
    """Clean up stale worker JSON files at the start of an --update run (master only)."""
    if hasattr(session.config, "workerinput"):
        return  # workers skip this
    try:
        update = session.config.getoption("--update")
    except ValueError:
        return
    if update:
        worker_logs = Path(__file__).parent / "worker_logs"
        for f in worker_logs.glob("*_mips_worker*.json"):
            f.unlink()

def pytest_generate_tests(metafunc):
    if "target" in metafunc.fixturenames:
        selected_arches = metafunc.config.getoption("arch")
        if isinstance(selected_arches, str):
            selected_arches = [selected_arches]
        metafunc.parametrize("target", selected_arches)


def pytest_sessionfinish(session, exitstatus):
    """
    Perform final aggregation and reference update (if run with --update).

    This hook runs once per process after all tests complete. The aggregation is done only
    on the master node (detected using, not hasattr(session.config, "workerinput")).
    It is expected to run on the master node, once all worker nodes have completed.

    When ``--update`` is specified:
    - All JSON result files updated by the workers are collected.
      Worker JSON files are expected in worker_logs/*_mips_worker*.json
    - The reference JSON and RST files is regenerated.
    """
     # master only; runs after all workers complete
    if not hasattr(session.config, "workerinput"):
        update = session.config.getoption("--update")
        if update: # update needs happen in pytest_sessionfinish after all worker nodes have run and written their corresponding <module>_mips_worker.json files
            # read all worker JSON files here — each contains {arch: {app: mips}}
            result_files = (Path(__file__).parent / "worker_logs").glob("*_mips_worker*.json")
            data = {}
            for f in result_files:
                for arch, apps in json.loads(f.read_text()).items():
                    data.setdefault(arch, {}).update(apps)

            # Merge with existing reference JSON so other architectures are preserved
            ref_json = Path(__file__).parent / "lib_voice_mips.json"
            if ref_json.exists():
                existing = json.loads(ref_json.read_text())
                for arch, apps in existing.items():
                    if arch not in data:
                        data[arch] = apps

            print(f"MIPS for all apps = {data}")
            # generate updated JSON
            with ref_json.open("w") as fp:
                json.dump(data, fp, indent=2)
            # generate updated RST
            rst_out = Path(__file__).parent / "lib_voice_mips_table.rst"
            write_rst_table(data, rst_out)
