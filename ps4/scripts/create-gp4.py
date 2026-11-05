#!/usr/bin/env python3
"""Generate the GP4 project manifest consumed by OpenOrbis PkgTool.Core."""

import argparse
from datetime import datetime
from pathlib import Path
import xml.etree.ElementTree as ET


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--stage", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--content-id", required=True)
    args = parser.parse_args()

    stage = args.stage.resolve()
    files = sorted(
        path.relative_to(stage).as_posix()
        for path in stage.rglob("*")
        if path.is_file() and path.resolve() != args.out.resolve()
    )

    project = ET.Element(
        "psproject",
        {
            "xmlns:xsd": "http://www.w3.org/2001/XMLSchema",
            "xmlns:xsi": "http://www.w3.org/2001/XMLSchema-instance",
            "fmt": "gp4",
            "version": "1000",
        },
    )
    volume = ET.SubElement(project, "volume")
    ET.SubElement(volume, "volume_type").text = "pkg_ps4_app"
    ET.SubElement(volume, "volume_id").text = "PS4VOLUME"
    ET.SubElement(volume, "volume_ts").text = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    ET.SubElement(
        volume,
        "package",
        {
            "content_id": args.content_id,
            "passcode": "00000000000000000000000000000000",
            "storage_type": "digital50",
            "app_type": "full",
        },
    )
    chunks = ET.SubElement(volume, "chunk_info", {"chunk_count": "1", "scenario_count": "1"})
    chunk_list = ET.SubElement(chunks, "chunks")
    ET.SubElement(chunk_list, "chunk", {"id": "0", "layer_no": "0", "label": "Chunk #0"})
    scenarios = ET.SubElement(chunks, "scenarios", {"default_id": "0"})
    ET.SubElement(
        scenarios,
        "scenario",
        {"id": "0", "type": "sp", "initial_chunk_count": "1", "label": "Scenario #0"},
    ).text = "0"

    file_list = ET.SubElement(project, "files", {"img_no": "0"})
    root_dirs: dict[str, dict] = {}
    for filename in files:
        ET.SubElement(file_list, "file", {"targ_path": filename, "orig_path": filename})
        parts = filename.split("/")[:-1]
        children = root_dirs
        for part in parts:
            children = children.setdefault(part, {})

    def append_dirs(parent: ET.Element, dirs: dict[str, dict]) -> None:
        for name in sorted(dirs):
            node = ET.SubElement(parent, "dir", {"targ_name": name})
            append_dirs(node, dirs[name])

    root_dir = ET.SubElement(project, "rootdir")
    append_dirs(root_dir, root_dirs)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    ET.ElementTree(project).write(args.out, encoding="utf-8", xml_declaration=True)


if __name__ == "__main__":
    main()
