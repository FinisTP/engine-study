# Browser checks of the web build (P10) in headless Chrome (Playwright). Serve the page on
# localhost:8765 first (python3 -m http.server 8765 -d <build>/apps/umbra), then: python3 check-web.py. Rendering itself is not judged here
# (this container's software GPU loses WebGPU devices); everything around it is.
import asyncio, sys
from playwright.async_api import async_playwright
URL = 'http://localhost:8765/index.html'
GPU = ['--enable-unsafe-webgpu', '--enable-features=Vulkan,WebGPU', '--use-webgpu-adapter=swiftshader', '--enable-unsafe-swiftshader', '--no-sandbox']
results = []
def check(name, ok, detail=''):
    results.append(ok); print(('PASS ' if ok else 'FAIL ') + name + (' -- ' + str(detail) if detail else ''))

async def ready(page):
    for _ in range(60):
        if await page.evaluate('typeof Module !== "undefined" && (Module.frameCount || 0) > 20'): return True
        await page.wait_for_timeout(500)
    return False

async def main():
    async with async_playwright() as p:
        browser = await p.chromium.launch(executable_path='/usr/bin/google-chrome', headless=True, args=GPU)
        ctx = await browser.new_context(viewport={'width': 1280, 'height': 720})
        page = await ctx.new_page()
        errors = []
        page.on('pageerror', lambda e: errors.append(str(e)))
        await page.goto(URL)
        check('game starts and draws frames', await ready(page))
        check('HUD shows the hero', 'HP 110/110' in await page.inner_text('#status'))
        check('no HUD warnings (scene and content loaded)', (await page.inner_text('#warnings')).strip() == '', await page.inner_text('#warnings'))
        # (this container loses the WebGPU device, which shows the 'device lost' screen: hide it)
        await page.evaluate("document.getElementById('start').style.display='none'; document.getElementById('canvas').focus()")
        # Input: Tab switches the combat mode (GameFlow), shown in the HUD.
        await page.keyboard.press('Tab'); await page.wait_for_timeout(500)
        check('keyboard reaches the game (Tab -> real time)', 'real-time' in await page.inner_text('#status'), await page.inner_text('#status'))
        # Escape opens the pause menu (HTML menu from core::Hud).
        await page.keyboard.press('Escape'); await page.wait_for_timeout(500)
        check('pause menu as HTML', await page.is_visible('#menu') and 'Paused' in await page.inner_text('#menu-title'))
        await page.keyboard.press('Escape'); await page.wait_for_timeout(300)
        # Saves: a file in the saves folder survives a reload (IndexedDB through IDBFS).
        await page.evaluate("FS.mkdirTree('/libsdl/Umbra/UmbraEngine/saves'); FS.writeFile('/libsdl/Umbra/UmbraEngine/saves/probe.txt', 'kept'); Module.persistSaves();")
        await page.wait_for_timeout(1000)
        await page.reload(); await ready(page)
        kept = await page.evaluate("(() => { try { return FS.readFile('/libsdl/Umbra/UmbraEngine/saves/probe.txt', {encoding: 'utf8'}); } catch (e) { return String(e); } })()")
        check('saves survive a reload (IndexedDB)', kept == 'kept', kept)
        # Offline: the service worker reports one complete version; with the network off the page still starts.
        for _ in range(20):
            label = await page.inner_text('#offline')
            if label.startswith('Ready offline'): break
            await page.wait_for_timeout(500)
        check('offline install reported ready', label.startswith('Ready offline'), label)
        await ctx.set_offline(True)
        await page.reload(); started = await ready(page)
        check('starts with the network off', started)
        await ctx.set_offline(False)
        check('no script errors', not errors, errors[:3])
        # Mismatched files: umbra.data from another build -> a clear message, the game doesn't run.
        ctx2 = await browser.new_context()  # no service worker from the first context
        page2 = await ctx2.new_page()
        await page2.route('**/umbra.data', lambda route: route.fulfill(body=b'RIFF' + b'\0' * 3259776, content_type='application/octet-stream'))
        await page2.goto(URL); await page2.wait_for_timeout(5000)
        text = await page2.inner_text('#start-text')
        check('mismatched umbra.data: clear message', 'do not belong together' in text, text)
        await browser.close()
        # No WebGPU: a clear message, the game is not loaded.
        browser = await p.chromium.launch(executable_path='/usr/bin/google-chrome', headless=True, args=['--disable-features=WebGPU', '--disable-gpu', '--no-sandbox'])
        page = await browser.new_page()
        await page.add_init_script("Object.defineProperty(Navigator.prototype, 'gpu', { get: () => undefined })") # a browser without WebGPU
        await page.goto(URL); await page.wait_for_timeout(2000)
        text = await page.inner_text('#start-text')
        has = await page.evaluate('!!navigator.gpu')
        loaded = await page.evaluate('typeof Module !== "undefined" && !!Module.calledRun')
        check('without WebGPU: clear message, game not loaded', not has and 'no WebGPU' in text and not loaded, text)
        await browser.close()
    print(f'{sum(results)}/{len(results)} passed')
asyncio.run(main())
