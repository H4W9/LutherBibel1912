# generate_sd_xml.py

Converts an OSIS XML Bible file into a folder tree of plain-text verse files
for use on a Flipper Zero SD card.

OSIS XML Bible files available here
https://github.com/gratis-bible/bible

## Output structure

```
<bible_name>/
  <Section>/
    <Book>/
      <Chapter>/
        verse1.txt
        verse2.txt
        ...
```

A zip archive (`<bible_name>_sd.zip`) is also created alongside the folder.

---

## Usage

Place `generate_sd_xml.py` in the same folder as your XML file, then open a
terminal in that folder.

**Basic — let the script auto-detect the language:**
```
python3 generate_sd_xml.py yourfile.xml
```

**Force English folder names:**
```
python3 generate_sd_xml.py yourfile.xml --lang en
```

**Force German folder names:**
```
python3 generate_sd_xml.py yourfile.xml --lang de
```

If `--lang` is omitted the script reads the language tag inside the XML and
picks English or German automatically. Most English Bibles (ASV, WEB, MKJV)
will auto-detect as English; the Luther 1912 will auto-detect as German.

---

## Examples

| Command | Output folder | Language |
|---|---|---|
| `python3 generate_sd_xml.py luth1912ap.xml` | `luth1912ap/` | German (auto) |
| `python3 generate_sd_xml.py asv.xml` | `asv/` | English (auto) |
| `python3 generate_sd_xml.py web.xml` | `web/` | English (auto) |
| `python3 generate_sd_xml.py mkjv1962.xml` | `mkjv1962/` | English (auto) |
| `python3 generate_sd_xml.py asv.xml --lang de` | `asv/` | German (forced) |

---

## Copying to the Flipper Zero

Once the script finishes, copy the output folder to your Flipper SD card:

```
/ext/apps_data/<bible_name>/
```

For example, for `asv.xml` the folder goes to:

```
/ext/apps_data/asv/
```

You can copy the folder manually via USB, or unzip the `_sd.zip` archive and
copy its contents.

---

## Notes

- The script requires no third-party packages — only the Python standard library.
- Supported XML format: OSIS 2.1.1 (the same format used by the Free Bible
  Software Group / ZefToOsis encoder).
