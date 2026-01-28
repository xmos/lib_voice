# Copyright 2022-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
"""
Shared profiling utilities for parsing profile output from xcore tests.

This module provides common functions for:
- Extracting profile tags from source files using regex
- Parsing profile output from test executables
- Calculating timing statistics (cycles, MIPS/MCPS)
- Generating profile reports
"""

import re
import glob
from collections import defaultdict
from typing import Dict, List, Tuple, Optional


def extract_profile_strings_from_source(
    src_folder: str,
    file_extensions: Optional[List[str]] = None,
    recursive: bool = True
) -> Dict[str, str]:
    """
    Extract profile tag strings from source files.
    
    Searches for prof(index, "tag_string") calls in source files and builds
    a mapping from profile index to tag string.
    
    Args:
        src_folder: Path to folder containing source files
        file_extensions: List of file extensions to search (default: ['.xc', '.c'])
        recursive: Whether to search subdirectories recursively
        
    Returns:
        Dictionary mapping profile index (as string) to tag string
        
    Raises:
        AssertionError: If duplicate profile indexes are found
    """
    if file_extensions is None:
        file_extensions = ['.xc', '.c']
    
    profile_strings = {}
    profile_regex = re.compile(r'\s*prof\s*\(\s*(\d+)\s*,\s*"(.*)"\s*\)\s*;')
    
    # Find all source files that might have a prof() function call
    src_files = []
    for ext in file_extensions:
        pattern = f'{src_folder}/**/*{ext}' if recursive else f'{src_folder}/*{ext}'
        src_files.extend(glob.glob(pattern, recursive=recursive))
    
    for file in src_files:
        with open(file, 'r') as fd:
            lines = fd.readlines()
        for line in lines:
            # Look for prof(profiling_index, tag_string) type of calls
            m = profile_regex.match(line)
            if m:
                if m.group(1) in profile_strings:
                    print(f"Profiling index {m.group(1)} used more than once with tags "
                          f"'{profile_strings[m.group(1)]}' and '{m.group(2)}'.")
                    assert False, "Duplicate profile index found"
                # Add to dict[profile_index] = tag_string structure
                profile_strings[m.group(1)] = m.group(2)
    
    return profile_strings


def parse_profile_output(
    prof_stdo: List[str],
    profile_strings: Dict[str, str]
) -> List[Dict[str, int]]:
    """
    Parse profile output and extract timing data for each frame.
    
    Args:
        prof_stdo: List of stdout lines from profiled executable
        profile_strings: Dictionary mapping profile index to tag string
        
    Returns:
        List of dictionaries, where each dict contains tag_string -> timer_snapshot
        mappings for one frame
    """
    all_frames = []
    tags = {}  # Dictionary that stores dict[tag_string] = timer_snapshot information
    profile_regex = re.compile(r'Profile\s*(\d+)\s*,\s*(\d+)')
    frame_regex = re.compile(r'frame\s*(\d+)')
    frame_num = 0
    
    for line in prof_stdo:
        m = frame_regex.match(line)
        if m:
            if frame_num:
                # Append previous frame's profiling info to all_frames
                all_frames.append(tags)
                tags = {}  # Reset tags
            frame_num += 1
        m = profile_regex.match(line)
        if m:
            prof_index = m.group(1)
            if prof_index in profile_strings:
                tags[profile_strings[prof_index]] = int(m.group(2))
    
    # Don't forget the last frame
    if tags:
        all_frames.append(tags)
    
    return all_frames


def calculate_frame_cycles(
    frame_tags: Dict[str, int],
    exclude_init: bool = False
) -> Tuple[Dict[str, int], int]:
    """
    Calculate cycles between start_ and end_ tags for a frame.
    
    Args:
        frame_tags: Dictionary of tag_string -> timer_snapshot for a frame
        exclude_init: Whether to exclude tags ending with 'init' from total
        
    Returns:
        Tuple of (dict mapping tag to cycles, total cycles)
    """
    cycles_dict = {}
    total_cycles = 0
    
    for tag in frame_tags:
        if tag.startswith('start_'):
            tag_string = tag[6:]  # Extract string after 'start_'
            end_tag = 'end_' + tag_string
            if end_tag in frame_tags:
                cycles = frame_tags[end_tag] - frame_tags[tag]
                cycles_dict[tag_string] = cycles
                # Exclude init processing if requested
                if not (exclude_init and tag.endswith('init')):
                    total_cycles += cycles
    
    return cycles_dict, total_cycles


def find_worst_case_frame(
    all_frames: List[Dict[str, int]],
    exclude_init: bool = False
) -> Tuple[Dict[str, int], int, int]:
    """
    Find the frame with the worst-case (maximum) total cycles.
    
    Args:
        all_frames: List of frame_tags dictionaries
        exclude_init: Whether to exclude init tags from total calculation
        
    Returns:
        Tuple of (cycles_dict, total_cycles, frame_number)
    """
    worst_case = (None, 0, -1)
    
    for frame_num, frame_tags in enumerate(all_frames):
        cycles_dict, total_cycles = calculate_frame_cycles(frame_tags, exclude_init)
        if total_cycles > worst_case[1]:
            worst_case = (cycles_dict, total_cycles, frame_num)
    
    return worst_case


def find_worst_case_per_tag(
    all_frames: List[Dict[str, int]]
) -> Dict[str, Dict[str, int]]:
    """
    Find worst-case cycles for each tag across all frames.
    
    Args:
        all_frames: List of frame_tags dictionaries
        
    Returns:
        Dictionary mapping tag_string to dict with 'cycles' and 'frame_num'
    """
    worst_case_dict = defaultdict(lambda: {'cycles': 0, 'frame_num': -1})
    
    for frame_num, frame_tags in enumerate(all_frames):
        for tag in frame_tags:
            if tag.startswith('start_'):
                tag_string = tag[6:]
                end_tag = 'end_' + tag_string
                if end_tag in frame_tags:
                    cycles = frame_tags[end_tag] - frame_tags[tag]
                    if worst_case_dict[tag_string]['cycles'] < cycles:
                        worst_case_dict[tag_string] = {
                            'cycles': cycles,
                            'frame_num': frame_num
                        }
    
    return worst_case_dict


def calculate_mips(
    processor_cycles: float,
    seconds_per_frame: float = 0.015,
) -> float:
    """
    Calculate MIPS (Million Instructions Per Second).
    
    Args:
        processor_cycles: Number of processor cycles in the frame
        seconds_per_frame: Duration of one frame in seconds
        
    Returns:
        MIPS value
    """
    frames_per_second = 1.0 / seconds_per_frame
    cycles_per_second = processor_cycles * frames_per_second
    mips = cycles_per_second / 1_000_000
    return mips


def timer_ticks_to_processor_cycles(
    timer_ticks: int,
    timer_mhz: float = 100.0,
    processor_mhz: float = 120.0
) -> float:
    """
    Convert timer ticks to processor cycles.
    
    Args:
        timer_ticks: Number of timer ticks (100MHz timer)
        timer_mhz: Timer frequency in MHz
        processor_mhz: Processor frequency in MHz
        
    Returns:
        Number of processor cycles
    """
    return (timer_ticks / timer_mhz) * processor_mhz


def write_detailed_profile_log(
    all_frames: List[Dict[str, int]],
    profile_file: str = "parsed_profile.log",
    exclude_init: bool = False
) -> None:
    """
    Write detailed per-frame profile log.
    
    Args:
        all_frames: List of frame_tags dictionaries
        profile_file: Output file path
        exclude_init: Whether to exclude init tags from totals
    """
    with open(profile_file, 'w') as fp:
        fp.write(f'{"Tag":<44} {"Cycles":<12} {"% of total cycles":<10}\n')
        
        for frame_num, frame_tags in enumerate(all_frames):
            fp.write(f"Frame {frame_num}\n")
            cycles_dict, total_cycles = calculate_frame_cycles(frame_tags, exclude_init)
            
            for key, value in cycles_dict.items():
                if total_cycles > 0:
                    percentage = round((value / float(total_cycles)) * 100, 2)
                else:
                    percentage = 0.0
                fp.write(f'{key:<44} {value:<12} {percentage:>10}% \n')
            fp.write(f'{"TOTAL_CYCLES":<32} {total_cycles}\n')


def write_worst_case_report(
    worst_case_cycles: Dict[str, int],
    total_cycles: int,
    frame_num: int,
    worst_case_file: str = "worst_case.log",
    config_info: Optional[str] = None,
    init_cycles: Optional[int] = None,
    timer_mhz: float = 100.0,
    processor_mhz: float = 120.0,
    seconds_per_frame: float = 0.015
) -> None:
    """
    Write worst-case frame report with timing statistics.
    
    Args:
        worst_case_cycles: Dictionary mapping tag to cycles for worst-case frame
        total_cycles: Total cycles in worst-case frame
        frame_num: Frame number of worst case
        worst_case_file: Output file path
        config_info: Optional configuration information string
        init_cycles: Optional initialization cycles to report separately
        timer_mhz: Timer frequency in MHz
        processor_mhz: Processor frequency in MHz
        seconds_per_frame: Duration of one frame in seconds
    """
    with open(worst_case_file, 'w') as fp:
        if config_info:
            fp.write(f"{config_info}\n")
        
        fp.write(f"Worst case frame = {frame_num}\n")
        
        if init_cycles is not None:
            fp.write(f"{'init':<44} {init_cycles:<12}\n")
        
        # Write individual tag cycles and percentages
        for key, value in worst_case_cycles.items():
            if not (init_cycles is not None and 'init' in key):
                if total_cycles > 0:
                    percentage = round((value / float(total_cycles)) * 100, 2)
                else:
                    percentage = 0.0
                fp.write(f'{key:<44} {value:<12} {percentage:>10}% \n')
        
        # Calculate and write timing statistics
        worst_case_timer_ticks = int(total_cycles)
        fp.write(f'{f"Worst_case_frame_timer({timer_mhz}MHz)_ticks":<44} {worst_case_timer_ticks}\n')
        
        worst_case_processor_cycles = timer_ticks_to_processor_cycles(
            worst_case_timer_ticks, timer_mhz, processor_mhz
        )
        fp.write(f'{f"Worst_case_frame_processor({processor_mhz}MHz)_cycles":<44} '
                 f'{int(worst_case_processor_cycles)}\n')
        
        mips = calculate_mips(worst_case_processor_cycles, seconds_per_frame)
        fp.write(f'{"MCPS":<44} {mips:.2f} MIPS\n')


def write_worst_case_per_tag_report(
    worst_case_dict: Dict[str, Dict[str, int]],
    worst_case_file: str = "worst_case.log",
    timer_mhz: float = 100.0,
    processor_mhz: float = 120.0,
    seconds_per_frame: float = 0.015
) -> None:
    """
    Write worst-case report showing worst case for each tag across all frames.
    
    This is an alternative reporting style used by test_vnr_profile.
    
    Args:
        worst_case_dict: Dictionary mapping tag to worst-case info
        worst_case_file: Output file path
        timer_mhz: Timer frequency in MHz
        processor_mhz: Processor frequency in MHz
        seconds_per_frame: Duration of one frame in seconds
    """
    total_cycles = sum(val['cycles'] for val in worst_case_dict.values())
    
    with open(worst_case_file, 'w') as fp:
        name = "Function"
        worst_case = "worst_case_frame"
        cycles = f"{timer_mhz}MHz_timer_ticks"
        mcps = f"{processor_mhz}MHz_processor_MCPS"
        percent = "%_total"
        fp.write(f'{name:<24} {worst_case:<24} {cycles:<24} {mcps:<26} {percent}\n')
        
        for key, val in worst_case_dict.items():
            worst_case_frame = val['frame_num']
            worst_case_cycles_timer = val['cycles']
            if total_cycles > 0:
                percentage_total = (float(worst_case_cycles_timer) / total_cycles) * 100
            else:
                percentage_total = 0.0
            
            processor_cycles = timer_ticks_to_processor_cycles(
                worst_case_cycles_timer, timer_mhz, processor_mhz
            )
            mcps_value = calculate_mips(processor_cycles, seconds_per_frame)
            
            fp.write(f'{key:<24}: {worst_case_frame:<26} '
                     f'{round(worst_case_cycles_timer, 2):<24} '
                     f'{round(mcps_value, 3):<24} {round(percentage_total, 3)}%\n')


def parse_profile_log(
    prof_stdo: List[str],
    src_folder: str,
    profile_file: str = "parsed_profile.log",
    worst_case_file: str = "worst_case.log",
    mapping_file: str = "profile_index_to_tag_mapping.log",
    config_info: Optional[str] = None,
    exclude_init: bool = False,
    per_tag_worst_case: bool = False,
    file_extensions: Optional[List[str]] = None,
    recursive: bool = True,
    timer_mhz: float = 100.0,
    processor_mhz: float = 120.0,
    seconds_per_frame: float = 0.015
) -> None:
    """
    Complete profile log parsing and report generation.
    
    This is a high-level function that combines all the steps:
    1. Extract profile strings from source files
    2. Parse profile output
    3. Calculate timing statistics
    4. Generate reports
    
    Args:
        prof_stdo: List of stdout lines from profiled executable
        src_folder: Path to source folder containing prof() calls
        profile_file: Output file for detailed per-frame profile
        worst_case_file: Output file for worst-case summary
        mapping_file: Output file for index-to-tag mapping
        config_info: Optional configuration info to include in report
        exclude_init: Whether to exclude init tags from totals
        per_tag_worst_case: Use per-tag worst-case reporting (vnr style)
        file_extensions: List of file extensions to search
        recursive: Whether to search subdirectories recursively
        timer_mhz: Timer frequency in MHz
        processor_mhz: Processor frequency in MHz
        seconds_per_frame: Duration of one frame in seconds
    """
    # Extract profile strings from source
    profile_strings = extract_profile_strings_from_source(
        src_folder, file_extensions, recursive
    )
    
    # Save index mapping
    with open(mapping_file, 'w') as fp:
        for index in profile_strings:
            fp.write(f'{index:<4} {profile_strings[index]}\n')
    
    # Parse profile output
    all_frames = parse_profile_output(prof_stdo, profile_strings)
    
    # Write detailed profile log
    write_detailed_profile_log(all_frames, profile_file, exclude_init)
    
    # Generate worst-case report
    if per_tag_worst_case:
        # VNR-style: worst case for each tag across all frames
        worst_case_dict = find_worst_case_per_tag(all_frames)
        write_worst_case_per_tag_report(
            worst_case_dict, worst_case_file, timer_mhz, processor_mhz, seconds_per_frame
        )
    else:
        # Standard style: worst-case frame
        worst_case_cycles, total_cycles, frame_num = find_worst_case_frame(
            all_frames, exclude_init
        )
        
        # Extract init cycles if needed
        init_cycles = None
        if exclude_init and all_frames:
            for tag in all_frames[0]:
                if tag.startswith('start_') and tag.endswith('init'):
                    end_tag = 'end_' + tag[6:]
                    if end_tag in all_frames[0]:
                        init_cycles = all_frames[0][end_tag] - all_frames[0][tag]
                        break
        
        write_worst_case_report(
            worst_case_cycles, total_cycles, frame_num, worst_case_file,
            config_info, init_cycles, timer_mhz, processor_mhz, seconds_per_frame
        )
