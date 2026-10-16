import os, sys
from playwright.sync_api import sync_playwright
here = os.path.dirname(os.path.abspath(__file__))
pairs = [a.split('=') for a in sys.argv[1:]]
with sync_playwright() as p:
    b = p.chromium.launch()
    pg = b.new_page(viewport={"width": 1200, "height": 640}, device_scale_factor=2)
    for src, dst in pairs:
        pg.goto("file://" + os.path.join(here, "source", src))
        pg.wait_for_timeout(400)
        pg.screenshot(path=os.path.join(here, dst), full_page=False)
        print("wrote", dst)
    b.close()
