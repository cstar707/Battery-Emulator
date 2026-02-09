# Offline Tesla Service Manual

Download and view the Tesla Service Manual (Model S or Model 3, English only) offline.

- **Model S:** [service.tesla.com/docs/ModelS/ServiceManual/en-us](https://service.tesla.com/docs/ModelS/ServiceManual/en-us/)
- **Model 3:** [service.tesla.com/docs/Model3/ServiceManual/en-us](https://service.tesla.com/docs/Model3/ServiceManual/en-us/) (language picker: [index](https://service.tesla.com/docs/Model3/ServiceManual/index-model-3-2017.html))

1. **Install wget** (if needed):
   ```bash
   brew install wget
   ```

2. **Run the download script** from the repo root:
   ```bash
   ./scripts/download-tesla-service-manual.sh          # Model S
   ./scripts/download-tesla-service-manual.sh Model3   # Model 3 (English)
   ```
   Output: `docs/offline-manuals/ModelS-ServiceManual/` or `Model3-ServiceManual/` (gitignored).

   The script is set up to get the **entire manual**:
   - Follows **all links** (unlimited recursion) under the manual path
   - Downloads **page requisites** (CSS, JS, images, fonts under the same path)
   - Stays on **service.tesla.com** only (no external domains)
   - Ignores **robots.txt** so no manual pages are skipped
   - Re-run the script to **resume or refresh**; wget skips already-downloaded files

3. **View the manual** (pick one):
   - **Recommended – local server** (links work best):  
     `./scripts/serve-tesla-service-manual.sh` (Model S) or `./scripts/serve-tesla-service-manual.sh Model3`  
     Then open the URL it prints (e.g. **http://localhost:8000/service.tesla.com/docs/ModelS/ServiceManual/en-us/**).
   - **Or open the file directly**:  
     `open "docs/offline-manuals/ModelS-ServiceManual/service.tesla.com/docs/ModelS/ServiceManual/en-us/index.html"`  
     (Use `Model3-ServiceManual` and `Model3` in the path for Model 3.)
   - Sections only work once their page is downloaded. Let the download script finish so all TOC pages are present.

**Custom output directory:**
```bash
./scripts/download-tesla-service-manual.sh /path/to/my/manual
```

**Note:** The mirror is a static snapshot. Content loaded only via JavaScript (e.g. from external APIs) will not be included. Re-run the script periodically to refresh the copy.
