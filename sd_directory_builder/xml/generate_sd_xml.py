#!/usr/bin/env python3
"""
Generate Bible SD card directory tree for Flipper Zero from OSIS XML.

Automatically detects the Bible's language from the XML and uses English or
German folder names accordingly. You can also force a language with --lang.

Output structure:
  <out_dir>/
    <Section>/
      <Book>/
        <Chapter>/
          verse1.txt
          verse2.txt
          ...

Run:
  python3 generate_sd_xml.py luth1912ap.xml
  python3 generate_sd_xml.py asv.xml
  python3 generate_sd_xml.py web.xml --lang en   # force English
  python3 generate_sd_xml.py luth1912ap.xml --lang de  # force German

Output folder and zip name are derived from the input filename.
  e.g. asv.xml  ->  ./asv/  and  ./asv_sd.zip
"""

import os
import sys
import zipfile
import argparse
import xml.etree.ElementTree as ET

NS = {"osis": "http://www.bibletechnologies.net/2003/OSIS/namespace"}

# ── German book map: osisID -> (section_folder, book_folder) ──────────────────
BOOK_MAP_DE = {
    # ── Altes Testament ──────────────────────────────────────────────────────
    "Gen":     ("Altes_Testament", "1_Mose"),
    "Exod":    ("Altes_Testament", "2_Mose"),
    "Lev":     ("Altes_Testament", "3_Mose"),
    "Num":     ("Altes_Testament", "4_Mose"),
    "Deut":    ("Altes_Testament", "5_Mose"),
    "Josh":    ("Altes_Testament", "Josua"),
    "Judg":    ("Altes_Testament", "Richter"),
    "Ruth":    ("Altes_Testament", "Ruth"),
    "1Sam":    ("Altes_Testament", "1_Samuel"),
    "2Sam":    ("Altes_Testament", "2_Samuel"),
    "1Kgs":    ("Altes_Testament", "1_Konige"),
    "2Kgs":    ("Altes_Testament", "2_Konige"),
    "1Chr":    ("Altes_Testament", "1_Chronik"),
    "2Chr":    ("Altes_Testament", "2_Chronik"),
    "Ezra":    ("Altes_Testament", "Esra"),
    "Neh":     ("Altes_Testament", "Nehemia"),
    "Esth":    ("Altes_Testament", "Esther"),
    "Job":     ("Altes_Testament", "Hiob"),
    "Ps":      ("Altes_Testament", "Psalm"),
    "Prov":    ("Altes_Testament", "Spruche"),
    "Eccl":    ("Altes_Testament", "Prediger"),
    "Song":    ("Altes_Testament", "Hohelied"),
    # ── Propheten ────────────────────────────────────────────────────────────
    "Isa":     ("Propheten", "Jesaja"),
    "Jer":     ("Propheten", "Jeremia"),
    "Lam":     ("Propheten", "Klagelieder"),
    "Ezek":    ("Propheten", "Hesekiel"),
    "Dan":     ("Propheten", "Daniel"),
    "Hos":     ("Propheten", "Hosea"),
    "Joel":    ("Propheten", "Joel"),
    "Amos":    ("Propheten", "Amos"),
    "Obad":    ("Propheten", "Obadja"),
    "Jonah":   ("Propheten", "Jona"),
    "Mic":     ("Propheten", "Micha"),
    "Nah":     ("Propheten", "Nahum"),
    "Hab":     ("Propheten", "Habakuk"),
    "Zeph":    ("Propheten", "Zefanja"),
    "Hag":     ("Propheten", "Haggai"),
    "Zech":    ("Propheten", "Sacharja"),
    "Mal":     ("Propheten", "Maleachi"),
    # ── Neues Testament ──────────────────────────────────────────────────────
    "Matt":    ("Neues_Testament", "Matthaus"),
    "Mark":    ("Neues_Testament", "Markus"),
    "Luke":    ("Neues_Testament", "Lukas"),
    "John":    ("Neues_Testament", "Johannes"),
    "Acts":    ("Neues_Testament", "Apostelgeschichte"),
    "Rom":     ("Neues_Testament", "Romer"),
    "1Cor":    ("Neues_Testament", "1_Korinther"),
    "2Cor":    ("Neues_Testament", "2_Korinther"),
    "Gal":     ("Neues_Testament", "Galater"),
    "Eph":     ("Neues_Testament", "Epheser"),
    "Phil":    ("Neues_Testament", "Philipper"),
    "Col":     ("Neues_Testament", "Kolosser"),
    "1Thess":  ("Neues_Testament", "1_Thessalonicher"),
    "2Thess":  ("Neues_Testament", "2_Thessalonicher"),
    "1Tim":    ("Neues_Testament", "1_Timotheus"),
    "2Tim":    ("Neues_Testament", "2_Timotheus"),
    "Titus":   ("Neues_Testament", "Titus"),
    "Phlm":    ("Neues_Testament", "Philemon"),
    "Heb":     ("Neues_Testament", "Hebraer"),
    "Jas":     ("Neues_Testament", "Jakobus"),
    "1Pet":    ("Neues_Testament", "1_Petrus"),
    "2Pet":    ("Neues_Testament", "2_Petrus"),
    "1John":   ("Neues_Testament", "1_Johannes"),
    "2John":   ("Neues_Testament", "2_Johannes"),
    "3John":   ("Neues_Testament", "3_Johannes"),
    "Jude":    ("Neues_Testament", "Judas"),
    "Rev":     ("Neues_Testament", "Offenbarung"),
    # ── Apokryphen ───────────────────────────────────────────────────────────
    "Jdt":     ("Apokryphen", "Judith"),
    "Wis":     ("Apokryphen", "Weisheit"),
    "Tob":     ("Apokryphen", "Tobias"),
    "Sir":     ("Apokryphen", "Sirach"),
    "Bar":     ("Apokryphen", "Baruch"),
    "1Macc":   ("Apokryphen", "1_Makkabaer"),
    "2Macc":   ("Apokryphen", "2_Makkabaer"),
    "AddEsth": ("Apokryphen", "Zusatze_Esther"),
    "AddDan":  ("Apokryphen", "Zusatze_Daniel"),
    "PrMan":   ("Apokryphen", "Gebet_Manasse"),
}

# ── English book map: osisID -> (section_folder, book_folder) ─────────────────
BOOK_MAP_EN = {
    # ── Old Testament ────────────────────────────────────────────────────────
    "Gen":     ("Old_Testament", "Genesis"),
    "Exod":    ("Old_Testament", "Exodus"),
    "Lev":     ("Old_Testament", "Leviticus"),
    "Num":     ("Old_Testament", "Numbers"),
    "Deut":    ("Old_Testament", "Deuteronomy"),
    "Josh":    ("Old_Testament", "Joshua"),
    "Judg":    ("Old_Testament", "Judges"),
    "Ruth":    ("Old_Testament", "Ruth"),
    "1Sam":    ("Old_Testament", "1_Samuel"),
    "2Sam":    ("Old_Testament", "2_Samuel"),
    "1Kgs":    ("Old_Testament", "1_Kings"),
    "2Kgs":    ("Old_Testament", "2_Kings"),
    "1Chr":    ("Old_Testament", "1_Chronicles"),
    "2Chr":    ("Old_Testament", "2_Chronicles"),
    "Ezra":    ("Old_Testament", "Ezra"),
    "Neh":     ("Old_Testament", "Nehemiah"),
    "Esth":    ("Old_Testament", "Esther"),
    "Job":     ("Old_Testament", "Job"),
    "Ps":      ("Old_Testament", "Psalms"),
    "Prov":    ("Old_Testament", "Proverbs"),
    "Eccl":    ("Old_Testament", "Ecclesiastes"),
    "Song":    ("Old_Testament", "Song_of_Solomon"),
    # ── Prophets ─────────────────────────────────────────────────────────────
    "Isa":     ("Prophets", "Isaiah"),
    "Jer":     ("Prophets", "Jeremiah"),
    "Lam":     ("Prophets", "Lamentations"),
    "Ezek":    ("Prophets", "Ezekiel"),
    "Dan":     ("Prophets", "Daniel"),
    "Hos":     ("Prophets", "Hosea"),
    "Joel":    ("Prophets", "Joel"),
    "Amos":    ("Prophets", "Amos"),
    "Obad":    ("Prophets", "Obadiah"),
    "Jonah":   ("Prophets", "Jonah"),
    "Mic":     ("Prophets", "Micah"),
    "Nah":     ("Prophets", "Nahum"),
    "Hab":     ("Prophets", "Habakkuk"),
    "Zeph":    ("Prophets", "Zephaniah"),
    "Hag":     ("Prophets", "Haggai"),
    "Zech":    ("Prophets", "Zechariah"),
    "Mal":     ("Prophets", "Malachi"),
    # ── New Testament ────────────────────────────────────────────────────────
    "Matt":    ("New_Testament", "Matthew"),
    "Mark":    ("New_Testament", "Mark"),
    "Luke":    ("New_Testament", "Luke"),
    "John":    ("New_Testament", "John"),
    "Acts":    ("New_Testament", "Acts"),
    "Rom":     ("New_Testament", "Romans"),
    "1Cor":    ("New_Testament", "1_Corinthians"),
    "2Cor":    ("New_Testament", "2_Corinthians"),
    "Gal":     ("New_Testament", "Galatians"),
    "Eph":     ("New_Testament", "Ephesians"),
    "Phil":    ("New_Testament", "Philippians"),
    "Col":     ("New_Testament", "Colossians"),
    "1Thess":  ("New_Testament", "1_Thessalonians"),
    "2Thess":  ("New_Testament", "2_Thessalonians"),
    "1Tim":    ("New_Testament", "1_Timothy"),
    "2Tim":    ("New_Testament", "2_Timothy"),
    "Titus":   ("New_Testament", "Titus"),
    "Phlm":    ("New_Testament", "Philemon"),
    "Heb":     ("New_Testament", "Hebrews"),
    "Jas":     ("New_Testament", "James"),
    "1Pet":    ("New_Testament", "1_Peter"),
    "2Pet":    ("New_Testament", "2_Peter"),
    "1John":   ("New_Testament", "1_John"),
    "2John":   ("New_Testament", "2_John"),
    "3John":   ("New_Testament", "3_John"),
    "Jude":    ("New_Testament", "Jude"),
    "Rev":     ("New_Testament", "Revelation"),
    # ── Apocrypha ────────────────────────────────────────────────────────────
    "Jdt":     ("Apocrypha", "Judith"),
    "Wis":     ("Apocrypha", "Wisdom_of_Solomon"),
    "Tob":     ("Apocrypha", "Tobit"),
    "Sir":     ("Apocrypha", "Sirach"),
    "Bar":     ("Apocrypha", "Baruch"),
    "1Macc":   ("Apocrypha", "1_Maccabees"),
    "2Macc":   ("Apocrypha", "2_Maccabees"),
    "AddEsth": ("Apocrypha", "Additions_to_Esther"),
    "AddDan":  ("Apocrypha", "Additions_to_Daniel"),
    "PrMan":   ("Apocrypha", "Prayer_of_Manasseh"),
}

LANG_MAPS = {"de": BOOK_MAP_DE, "en": BOOK_MAP_EN}


def detect_lang(root) -> str:
    """Read xml:lang from <osisText>; return 'de', 'en', or 'en' as default."""
    for el in root.iter(f"{{{NS['osis']}}}osisText"):
        lang = el.get("{http://www.w3.org/XML/1998/namespace}lang", "")
        if lang.startswith("de"):
            return "de"
        if lang.startswith("en"):
            return "en"
    return "en"


def strip_tags(element) -> str:
    """Return all text content inside an element, ignoring child tags."""
    return "".join(element.itertext()).strip()


def main():
    parser = argparse.ArgumentParser(description="Generate Flipper Zero Bible SD tree from OSIS XML.")
    parser.add_argument("xml_file", nargs="?", default="luth1912ap.xml", help="Path to OSIS XML file")
    parser.add_argument("--lang", choices=["en", "de"], default=None,
                        help="Force folder language (en/de). Auto-detected from XML if omitted.")
    args = parser.parse_args()

    xml_path = args.xml_file
    if not os.path.isfile(xml_path):
        print(f"ERROR: XML file not found: {xml_path}", file=sys.stderr)
        sys.exit(1)

    # Derive output names from input filename  (asv.xml -> asv / asv_sd.zip)
    base     = os.path.splitext(os.path.basename(xml_path))[0]
    out_dir  = base
    zip_name = f"{base}_sd.zip"

    print(f"Parsing {xml_path} ...")
    tree = ET.parse(xml_path)
    root = tree.getroot()

    lang     = args.lang or detect_lang(root)
    book_map = LANG_MAPS[lang]
    print(f"  Language : {lang}  ({'auto-detected' if args.lang is None else 'forced'})")
    print(f"  Output   : ./{out_dir}/  and  ./{zip_name}")

    verse_count = 0
    file_count  = 0
    dirs_made   = set()
    skipped     = set()

    with zipfile.ZipFile(zip_name, "w", zipfile.ZIP_DEFLATED) as zf:
        for book_el in root.iter(f"{{{NS['osis']}}}div"):
            if book_el.get("type") != "book":
                continue
            osis_id = book_el.get("osisID", "")

            if osis_id not in book_map:
                if osis_id not in skipped:
                    print(f"  WARNING: Unknown book osisID '{osis_id}', skipping.")
                    skipped.add(osis_id)
                continue

            section_folder, book_folder = book_map[osis_id]

            for chapter_el in book_el.iter(f"{{{NS['osis']}}}chapter"):
                chapter_ref = chapter_el.get("osisID", "")
                chapter_num = chapter_ref.split(".")[-1] if "." in chapter_ref else chapter_ref

                for verse_el in chapter_el.iter(f"{{{NS['osis']}}}verse"):
                    verse_ref = verse_el.get("osisID", "")
                    parts     = verse_ref.split(".")
                    verse_num = parts[-1] if len(parts) >= 3 else verse_ref

                    text = strip_tags(verse_el)
                    if not text:
                        continue

                    chapter_dir = os.path.join(out_dir, section_folder, book_folder, chapter_num)
                    verse_file  = os.path.join(chapter_dir, f"verse{verse_num}.txt")

                    if chapter_dir not in dirs_made:
                        os.makedirs(chapter_dir, exist_ok=True)
                        dirs_made.add(chapter_dir)

                    with open(verse_file, "w", encoding="utf-8") as f:
                        f.write(text)

                    zip_path = verse_file.replace(out_dir + os.sep, out_dir + "/")
                    zf.write(verse_file, zip_path)

                    verse_count += 1
                    file_count  += 1

                    if verse_count % 5000 == 0:
                        print(f"  {verse_count} verses written...")

    print(f"\nDone!")
    print(f"  Verses written : {verse_count:,}")
    print(f"  Files created  : {file_count:,}")
    print(f"  Directories    : {len(dirs_made):,}")
    print(f"  Output folder  : ./{out_dir}/")
    print(f"  ZIP archive    : ./{zip_name}")
    print()
    print(f"Copy the '{out_dir}' folder to your Flipper SD card:")
    print(f"  /ext/apps_data/{out_dir}/")


if __name__ == "__main__":
    main()
