/**
 * PERFECT EMBED - INSTRUÇÕES DE USO
 * =============================================================================
 * Para embutir um SVG ou Imagem interativa, use os modelos de <div> abaixo:
 * 
 * MODELO PARA SVG:
 * <div class="svg-embed-container" 
 *      data-svg-path="../../assets/seu_diagrama.svg" 
 *      data-title="Título do Diagrama" 
 *      style="height: 600px; width: 100%;">
 * </div>
 * 
 * MODELO PARA IMAGEM (JPG/PNG):
 * <div class="image-embed-container" 
 *      data-image-path="../../assets/sua_imagem.png" 
 *      data-title="Título da Imagem" 
 *      style="height: 600px; width: 100%;">
 * </div>
 * =============================================================================
 */

// =============================================================================
// CONSTANTES
// =============================================================================

const ZOOM = Object.freeze({
    DEFAULT: 100,
    MIN: 25,
    MAX: 600,
    STEP: 25,
    TOUCH_PAN_SENSITIVITY: 0.7,
    TOUCH_ZOOM_DISCRETE_STEP: 10,
});

const ANIMATION = Object.freeze({
    BUTTON_FEEDBACK_MS: 250,
});

// =============================================================================
// UTILITÁRIOS
// =============================================================================

/**
 * Limita um valor entre um mínimo e um máximo.
 */
function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
}

/**
 * Associa um handler a um botão de ícone, aplicando o feedback visual padrão.
 */
function attachIconButton(buttonId, handler) {
    const btn = document.getElementById(buttonId);
    if (!btn) return;
    btn.onclick = () => {
        ButtonFeedback.apply(btn);
        handler();
    };
}

// =============================================================================
// MÓDULO: FEEDBACK DE BOTÃO
// =============================================================================

class ButtonFeedback {
    static apply(btn) {
        btn.classList.add("clicked");
        setTimeout(() => btn.classList.remove("clicked"), ANIMATION.BUTTON_FEEDBACK_MS);
    }
}

// =============================================================================
// MÓDULO: TELA CHEIA
// =============================================================================

class FullscreenHandler {
    static attach(buttonId, containerId) {
        const btn = document.getElementById(buttonId);
        const container = document.getElementById(containerId);

        if (!btn || !container) {
            console.warn(`FullscreenHandler: elementos '${buttonId}' ou '${containerId}' não encontrados.`);
            return;
        }

        btn.onclick = () => {
            ButtonFeedback.apply(btn);
            if (document.fullscreenElement) {
                document.exitFullscreen();
            } else {
                container.requestFullscreen().catch((err) =>
                    console.error(`Erro ao entrar em tela cheia: ${err.message}`)
                );
            }
        };
    }
}

// =============================================================================
// MÓDULO: DOWNLOAD
// =============================================================================

class DownloadHandler {
    static attachSvg(buttonId, svgElement, fileName) {
        const btn = document.getElementById(buttonId);
        if (!btn) return;

        btn.onclick = () => {
            ButtonFeedback.apply(btn);
            const blob = new Blob(
                [new XMLSerializer().serializeToString(svgElement)],
                { type: "image/svg+xml" }
            );
            DownloadHandler._triggerDownload(URL.createObjectURL(blob), `${fileName}.svg`);
        };
    }

    static attachImage(buttonId, imgElement, fileName, imagePath) {
        const btn = document.getElementById(buttonId);
        if (!btn) return;

        btn.onclick = () => {
            ButtonFeedback.apply(btn);
            const extension = imagePath.split(".").pop() || "png";
            DownloadHandler._triggerDownload(imgElement.src, `${fileName}.${extension}`);
        };
    }

    static _triggerDownload(href, fileName) {
        const anchor = Object.assign(document.createElement("a"), {
            href,
            download: fileName.replace(/\s/g, "_"),
        });
        anchor.click();
        if (href.startsWith("blob:")) URL.revokeObjectURL(href);
    }
}

// =============================================================================
// MÓDULO: ZOOM/PAN DE SVG
// =============================================================================

class SvgZoomPan {
    constructor(svgElement, canvasElement, zoomLabelElement, coordDisplayElement, initialViewBox) {
        this.svg = svgElement;
        this.canvas = canvasElement;
        this.zoomLabel = zoomLabelElement;
        this.coordDisplay = coordDisplayElement;

        this.initialViewBox = { ...initialViewBox };

        this.currentZoom = ZOOM.DEFAULT;
        this.offsetX = 0;
        this.offsetY = 0;
        this.viewWidth = 0;
        this.viewHeight = 0;
        this.scroll = 0;

        this._isPanning = false;
        this._startX = 0;
        this._startY = 0;
        this._lastTouchDist = null;

        this._applyZoom(ZOOM.DEFAULT, true);
        this._attachEventListeners();
    }

    // ---------------------------------------------------------------------------
    // API pública
    // ---------------------------------------------------------------------------

    simulateZoom(delta) {
        const newZoom = clamp(this.currentZoom + delta, ZOOM.MIN, ZOOM.MAX);
        if (newZoom === this.currentZoom) return;

        const oldFactor = this._zoomFactor(this.scroll);
        const newScroll = this._scrollFromZoom(newZoom);
        const scale = this._zoomFactor(newScroll) / oldFactor;

        const centerX = this.offsetX + this.viewWidth / 2;
        const centerY = this.offsetY + this.viewHeight / 2;

        this.viewWidth *= scale;
        this.viewHeight *= scale;
        this.offsetX = centerX - this.viewWidth / 2;
        this.offsetY = centerY - this.viewHeight / 2;

        this.scroll = newScroll;
        this.currentZoom = newZoom;
        this._syncView();
    }

    reset() {
        this._applyZoom(ZOOM.DEFAULT, true);
    }

    center() {
        const { x, y, width, height } = this.initialViewBox;
        this.offsetX = x + width / 2 - this.viewWidth / 2;
        this.offsetY = y + height / 2 - this.viewHeight / 2;
        this._updateViewBox();
    }

    // ---------------------------------------------------------------------------
    // Métodos privados
    // ---------------------------------------------------------------------------

    _zoomFactor(scroll) {
        return Math.pow(1.05, scroll / 100);
    }

    _scrollFromZoom(zoom) {
        return (Math.log(100 / zoom) / Math.log(1.05)) * 100;
    }

    _applyZoom(zoom, resetPan = false) {
        this.currentZoom = zoom;
        this.scroll = this._scrollFromZoom(zoom);

        const scale = 100 / zoom;
        const { x, y, width, height } = this.initialViewBox;

        this.viewWidth = width * scale;
        this.viewHeight = height * scale;

        if (resetPan) {
            this.offsetX = x + width / 2 - this.viewWidth / 2;
            this.offsetY = y + height / 2 - this.viewHeight / 2;
        }

        this._syncView();
    }

    _syncView() {
        this._updateViewBox();
        this._updateZoomLabel();
    }

    _updateViewBox() {
        this.svg.setAttribute(
            "viewBox",
            `${this.offsetX} ${this.offsetY} ${this.viewWidth} ${this.viewHeight}`
        );
    }

    _updateZoomLabel() {
        if (this.zoomLabel) {
            this.zoomLabel.textContent = `${Math.round(this.currentZoom)}%`;
        }
    }

    _updateCoordinates(clientX, clientY) {
        if (!this.coordDisplay) return;
        const rect = this.svg.getBoundingClientRect();
        const x = this.offsetX + ((clientX - rect.left) / rect.width) * this.viewWidth;
        const y = this.offsetY + ((clientY - rect.top) / rect.height) * this.viewHeight;
        this.coordDisplay.textContent = `X: ${x.toFixed(1)} Y: ${y.toFixed(1)}`;
    }

    _pan(dx, dy) {
        const scale = this.viewWidth / this.svg.getBoundingClientRect().width;
        this.offsetX -= dx * scale;
        this.offsetY -= dy * scale;
        this._updateViewBox();
    }

    _attachEventListeners() {
        this.canvas.addEventListener("wheel", (e) => {
            e.preventDefault();
            this.simulateZoom(e.deltaY > 0 ? -ZOOM.STEP : ZOOM.STEP);
        });

        this.canvas.addEventListener("mousedown", (e) => {
            this._isPanning = true;
            this._startX = e.clientX;
            this._startY = e.clientY;
            this.canvas.classList.add("dragging");
        });

        window.addEventListener("mouseup", () => this._stopPanning());
        window.addEventListener("mouseleave", () => this._stopPanning());

        this.canvas.addEventListener("mousemove", (e) => {
            this._updateCoordinates(e.clientX, e.clientY);
            if (!this._isPanning) return;

            this._pan(e.clientX - this._startX, e.clientY - this._startY);
            this._startX = e.clientX;
            this._startY = e.clientY;
        });

        this.canvas.addEventListener("touchstart", (e) => this._onTouchStart(e), { passive: false });
        this.canvas.addEventListener("touchmove", (e) => this._onTouchMove(e), { passive: false });
        this.canvas.addEventListener("touchend", (e) => this._onTouchEnd(e));
    }

    _stopPanning() {
        this._isPanning = false;
        this.canvas.classList.remove("dragging");
    }

    _onTouchStart(e) {
        if (e.touches.length === 1) {
            this._isPanning = true;
            this._startX = e.touches[0].clientX;
            this._startY = e.touches[0].clientY;
        } else if (e.touches.length === 2) {
            this._isPanning = false;
            this._lastTouchDist = this._touchDistance(e.touches);
        }
    }

    _onTouchMove(e) {
        e.preventDefault();

        if (e.touches.length === 1 && this._isPanning) {
            const dx = e.touches[0].clientX - this._startX;
            const dy = e.touches[0].clientY - this._startY;
            this._startX = e.touches[0].clientX;
            this._startY = e.touches[0].clientY;
            this._pan(dx, dy);
        } else if (e.touches.length === 2) {
            const dist = this._touchDistance(e.touches);
            if (this._lastTouchDist !== null) {
                this.simulateZoom((dist - this._lastTouchDist) * ZOOM.TOUCH_PAN_SENSITIVITY);
            }
            this._lastTouchDist = dist;
            this._isPanning = false;
        }
    }

    _onTouchEnd(e) {
        this._isPanning = false;
        if (e.touches.length < 2) this._lastTouchDist = null;
    }

    _touchDistance([t1, t2]) {
        return Math.hypot(t2.clientX - t1.clientX, t2.clientY - t1.clientY);
    }
}

// =============================================================================
// MÓDULO: ZOOM/PAN DE IMAGEM
// =============================================================================

class ImagePanZoom {
    constructor(imgElement, canvasElement, zoomLabelElement, coordDisplayElement) {
        this.img = imgElement;
        this.canvas = canvasElement;
        this.zoomLabel = zoomLabelElement;
        this.coordDisplay = coordDisplayElement;

        this.currentZoom = ZOOM.DEFAULT;
        this.panX = 0;
        this.panY = 0;

        this._isPanning = false;
        this._startX = 0;
        this._startY = 0;
        this._lastTouchDist = null;
        this._rafId = null;

        this._attachEventListeners();
        this._scheduleRender();
    }

    // ---------------------------------------------------------------------------
    // API pública
    // ---------------------------------------------------------------------------

    applyZoom(delta) {
        const newZoom = clamp(this.currentZoom + delta, ZOOM.MIN, ZOOM.MAX);
        if (newZoom === this.currentZoom) return;
        this.currentZoom = newZoom;
        this._scheduleRender();
    }

    reset() {
        this.currentZoom = ZOOM.DEFAULT;
        this.panX = 0;
        this.panY = 0;
        this._scheduleRender();
    }

    center() {
        this.panX = 0;
        this.panY = 0;
        this._scheduleRender();
    }

    // ---------------------------------------------------------------------------
    // Métodos privados
    // ---------------------------------------------------------------------------

    _clampedPan(panX, panY) {
        const scaledWidth = this.img.naturalWidth * (this.currentZoom / 100);
        const scaledHeight = this.img.naturalHeight * (this.currentZoom / 100);
        const { width, height } = this.canvas.getBoundingClientRect();

        return {
            x: clamp(panX, -Math.max(0, (scaledWidth - width) / 2), Math.max(0, (scaledWidth - width) / 2)),
            y: clamp(panY, -Math.max(0, (scaledHeight - height) / 2), Math.max(0, (scaledHeight - height) / 2)),
        };
    }

    _scheduleRender() {
        if (this._rafId) return;
        this._rafId = requestAnimationFrame(() => {
            this._render();
            this._rafId = null;
        });
    }

    _render() {
        const { x, y } = this._clampedPan(this.panX, this.panY);
        this.panX = x;
        this.panY = y;
        this.img.style.transform = `translate(${x}px, ${y}px) scale(${this.currentZoom / 100})`;
        if (this.zoomLabel) this.zoomLabel.textContent = `${Math.round(this.currentZoom)}%`;
    }

    _updateCoordinates(clientX, clientY) {
        if (!this.coordDisplay) return;
        const rect = this.canvas.getBoundingClientRect();
        const centerX = rect.width / 2 + this.panX;
        const centerY = rect.height / 2 + this.panY;
        const imgX = this.img.naturalWidth / 2 + (clientX - rect.left - centerX) / (this.currentZoom / 100);
        const imgY = this.img.naturalHeight / 2 + (clientY - rect.top - centerY) / (this.currentZoom / 100);
        this.coordDisplay.textContent = `X: ${Math.round(imgX)} Y: ${Math.round(imgY)}`;
    }

    _attachEventListeners() {
        this.canvas.addEventListener("wheel", (e) => {
            e.preventDefault();
            this.applyZoom(e.deltaY > 0 ? -ZOOM.STEP : ZOOM.STEP);
        });

        this.canvas.addEventListener("mousedown", (e) => {
            e.preventDefault();
            this._isPanning = true;
            this._startX = e.clientX;
            this._startY = e.clientY;
            this.canvas.classList.add("dragging");
        });

        window.addEventListener("mouseup", () => this._stopPanning());
        window.addEventListener("mouseleave", () => this._stopPanning());

        this.canvas.addEventListener("mousemove", (e) => {
            this._updateCoordinates(e.clientX, e.clientY);
            if (!this._isPanning) return;

            this.panX += e.clientX - this._startX;
            this.panY += e.clientY - this._startY;
            this._startX = e.clientX;
            this._startY = e.clientY;
            this._scheduleRender();
        });

        this.canvas.addEventListener("touchstart", (e) => this._onTouchStart(e), { passive: false });
        this.canvas.addEventListener("touchmove", (e) => this._onTouchMove(e), { passive: false });
        this.canvas.addEventListener("touchend", (e) => this._onTouchEnd(e));
    }

    _stopPanning() {
        this._isPanning = false;
        this.canvas.classList.remove("dragging");
    }

    _onTouchStart(e) {
        if (e.touches.length === 1) {
            this._isPanning = true;
            this._startX = e.touches[0].clientX;
            this._startY = e.touches[0].clientY;
        } else if (e.touches.length === 2) {
            this._isPanning = false;
            this._lastTouchDist = this._touchDistance(e.touches);
        }
    }

    _onTouchMove(e) {
        e.preventDefault();

        if (e.touches.length === 1 && this._isPanning) {
            this.panX += e.touches[0].clientX - this._startX;
            this.panY += e.touches[0].clientY - this._startY;
            this._startX = e.touches[0].clientX;
            this._startY = e.touches[0].clientY;
            this._scheduleRender();
        } else if (e.touches.length === 2) {
            const dist = this._touchDistance(e.touches);
            if (this._lastTouchDist !== null) {
                const delta = dist > this._lastTouchDist
                    ? ZOOM.TOUCH_ZOOM_DISCRETE_STEP
                    : -ZOOM.TOUCH_ZOOM_DISCRETE_STEP;
                this.applyZoom(delta);
            }
            this._lastTouchDist = dist;
            this._isPanning = false;
        }
    }

    _onTouchEnd(e) {
        this._isPanning = false;
        if (e.touches.length < 2) this._lastTouchDist = null;
    }

    _touchDistance([t1, t2]) {
        return Math.hypot(t2.clientX - t1.clientX, t2.clientY - t1.clientY);
    }
}

// =============================================================================
// MÓDULO: CONSTRUÇÃO DA UI
// =============================================================================

const EMBED_STYLE_TEMPLATE = `
  <style>
    @keyframes feedback-glow {
      0%   { transform: scale(1);    box-shadow: none; }
      50%  { transform: scale(1.1);  box-shadow: 0 0 15px var(--purple-accent); }
      100% { transform: scale(1);    box-shadow: none; }
    }

    .embed-container {
      width: 100%;
      height: 100%;
      display: flex;
      flex-direction: column;
      font-family: 'Roboto Mono', monospace !important;
      border: 1px solid var(--ui-border-subtle);
      background-color: var(--bg-surface);
      border-radius: 4px;
      overflow: hidden;
      box-shadow: 0 4px 20px rgba(0, 0, 0, 0.4);
    }

    .toolbar-top, .toolbar-bottom {
      min-height: 52px;
      display: flex;
      align-items: center;
      padding: 0 16px;
      box-sizing: border-box;
      background-color: var(--bg-header);
      color: #fff;
    }

    .toolbar-top {
      border-bottom: 1px solid var(--ui-border-subtle);
      justify-content: center;
    }

    .toolbar-bottom {
      border-top: 1px solid var(--ui-border-subtle);
      justify-content: space-between;
      gap: 12px;
    }

    .embed-title {
      font-size: 0.85rem;
      font-weight: 700;
      text-transform: uppercase;
      letter-spacing: 0.05em;
      font-style: italic;
      color: #fff;
    }

    .icon-btn {
      width: 36px;
      height: 36px;
      display: flex;
      align-items: center;
      justify-content: center;
      background: rgba(255, 255, 255, 0.03);
      border: 1px solid var(--ui-border-subtle);
      border-radius: 4px;
      cursor: pointer;
      color: var(--text-color);
      transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
    }

    .icon-btn:hover {
      background: rgba(147, 51, 234, 0.1);
      border-color: var(--purple-accent);
      color: var(--purple-light);
      transform: translateY(-1px);
    }

    .icon-btn .material-icons {
      font-size: 1.1rem;
      transition: transform 0.2s ease;
    }

    .icon-btn:active {
      transform: scale(0.95);
    }

    .icon-btn.clicked {
      animation: feedback-glow 0.3s ease;
      border-color: var(--purple-accent);
    }

    .zoom-label {
      font-size: 0.75rem;
      font-weight: 700;
      min-width: 45px;
      text-align: center;
      color: var(--purple-light);
    }

    .button-group, .coord-group {
      display: flex;
      align-items: center;
      gap: 6px;
    }

    .embed-canvas {
      flex: 1;
      overflow: hidden;
      display: flex;
      justify-content: center;
      align-items: center;
      background-image: 
        radial-gradient(circle at 2px 2px, rgba(255, 255, 255, 0.02) 1px, transparent 0);
      background-size: 24px 24px;
      cursor: grab;
    }

    .embed-canvas.dragging {
      cursor: grabbing;
    }

    @media (max-width: 600px) {
      .toolbar-bottom {
        flex-direction: column;
        height: auto;
        padding: 12px;
        gap: 12px;
      }
      .button-group, .coord-group {
        width: 100%;
        justify-content: center;
      }
    }
  </style>`;

class EmbedUIBuilder {
    static buildHtml(uniqueId, title, typePrefix) {
        const styles = EMBED_STYLE_TEMPLATE
            .replace(/__TYPE__/g, typePrefix)
            .replace(/__ID__/g, uniqueId);

        return `
      ${styles}
      <link href="https://fonts.googleapis.com/icon?family=Material+Icons" rel="stylesheet">
      <div id="${typePrefix}-container-${uniqueId}" class="embed-container">
        <div class="toolbar-top">
          <span id="${typePrefix}-title-${uniqueId}" class="embed-title">${title}</span>
        </div>
        <div id="canvas-area-${uniqueId}" class="embed-canvas"></div>
        <div class="toolbar-bottom">
          <div class="button-group">
            <button class="icon-btn" id="btn-${typePrefix}-zoom-out-${uniqueId}" title="Reduzir Zoom">
              <span class="material-icons">remove</span>
            </button>
            <span id="${typePrefix}-zoom-label-${uniqueId}" class="zoom-label">100%</span>
            <button class="icon-btn" id="btn-${typePrefix}-zoom-in-${uniqueId}" title="Aumentar Zoom">
              <span class="material-icons">add</span>
            </button>
            <button class="icon-btn" id="btn-${typePrefix}-reset-${uniqueId}" title="Resetar Zoom">
              <span class="material-icons">refresh</span>
            </button>
            <button class="icon-btn" id="btn-${typePrefix}-center-${uniqueId}" title="Centralizar">
              <span class="material-icons">center_focus_strong</span>
            </button>
          </div>
          <div class="coord-group">
            <button class="icon-btn" id="btn-${typePrefix}-download-${uniqueId}" title="Baixar">
              <span class="material-icons">download</span>
            </button>
            <button class="icon-btn" id="btn-${typePrefix}-fullscreen-${uniqueId}" title="Tela cheia">
              <span class="material-icons">fullscreen</span>
            </button>
          </div>
        </div>
      </div>`;
    }
}

// =============================================================================
// MÓDULO: EMBED DE SVG
// =============================================================================

class SvgEmbed {
    async create(rootElement) {
        if (!rootElement || rootElement.dataset.embedLoaded) return;
        rootElement.dataset.embedLoaded = "true";

        const svgPath = rootElement.dataset.svgPath;
        const title = rootElement.dataset.title || "Diagrama";

        if (!svgPath) {
            this._renderError(rootElement, "Atributo data-svg-path não definido.");
            return;
        }

        try {
            const absolutePath = new URL(svgPath, document.baseURI).href;
            const svg = await this._fetchSvg(absolutePath);

            const id = this._generateId("svg");
            this._prepareSvgElement(svg, id, absolutePath);

            rootElement.innerHTML = EmbedUIBuilder.buildHtml(id, title, "svg");
            document.getElementById(`canvas-area-${id}`).appendChild(svg);

            await this._waitForSvgFonts(svg);

            const viewBox = this._resolveViewBox(svg);
            this._fitSvgToCanvas(svg, viewBox);

            const controller = new SvgZoomPan(
                svg,
                document.getElementById(`canvas-area-${id}`),
                document.getElementById(`svg-zoom-label-${id}`),
                null,
                viewBox
            );

            this._attachControls(id, controller, svg, title);
        } catch (err) {
            this._renderError(rootElement, `Erro ao carregar SVG: ${err.message}`);
            console.error(err);
        }
    }

    // ---------------------------------------------------------------------------
    // Métodos privados
    // ---------------------------------------------------------------------------

    async _fetchSvg(absolutePath) {
        const res = await fetch(absolutePath);
        if (!res.ok) throw new Error(`HTTP ${res.status} ${res.statusText}`);
        const text = await res.text();
        const svg = new DOMParser().parseFromString(text, "image/svg+xml").querySelector("svg");
        if (!svg) throw new Error("O arquivo não contém um elemento <svg> válido.");
        return svg;
    }

    _prepareSvgElement(svg, id, absolutePath) {
        svg.setAttribute("id", id);
        this._prefixInternalIds(svg, id);
        this._resolveRelativeUrls(svg, absolutePath);
    }

    async _waitForSvgFonts(svg) {
        const fontPromises = [];

        svg.querySelectorAll("style").forEach((styleEl) => {
            const rules = styleEl.textContent.match(/@font-face\s*{[^}]*}/g) || [];
            rules.forEach((rule) => {
                const family = (rule.match(/font-family:\s*['"]?([^;'"}\s]+)['"]?/) || [])[1];
                if (!family) return;
                const weight = (rule.match(/font-weight:\s*([^;}\s]+)/) || ["", "normal"])[1];
                const style = (rule.match(/font-style:\s*([^;}\s]+)/) || ["", "normal"])[1];
                fontPromises.push(document.fonts.load(`${style} ${weight} 1em "${family}"`));
            });
        });

        if (fontPromises.length > 0) {
            await Promise.all(fontPromises).catch((err) =>
                console.warn("Falha ao carregar fontes do SVG. Layout pode estar incorreto.", err)
            );
        }
    }

    _resolveViewBox(svg) {
        const raw = svg.getAttribute("viewBox");
        if (raw) {
            const parts = raw.split(" ").map(Number);
            if (parts.length === 4 && parts.every(Number.isFinite)) {
                return { x: parts[0], y: parts[1], width: parts[2], height: parts[3] };
            }
        }

        const w = parseFloat(svg.getAttribute("width"));
        const h = parseFloat(svg.getAttribute("height"));
        if (w && h) return { x: 0, y: 0, width: w, height: h };

        const bbox = svg.getBBox();
        const pad = 10;
        return {
            x: bbox.x - pad,
            y: bbox.y - pad,
            width: bbox.width + 2 * pad,
            height: bbox.height + 2 * pad,
        };
    }

    _fitSvgToCanvas(svg, viewBox) {
        svg.setAttribute("viewBox", `${viewBox.x} ${viewBox.y} ${viewBox.width} ${viewBox.height}`);
        svg.setAttribute("width", "100%");
        svg.setAttribute("height", "100%");
        svg.setAttribute("preserveAspectRatio", "xMidYMid meet");
    }

    _attachControls(id, controller, svg, title) {
        attachIconButton(`btn-svg-zoom-in-${id}`, () => controller.simulateZoom(ZOOM.STEP));
        attachIconButton(`btn-svg-zoom-out-${id}`, () => controller.simulateZoom(-ZOOM.STEP));
        attachIconButton(`btn-svg-reset-${id}`, () => controller.reset());
        attachIconButton(`btn-svg-center-${id}`, () => controller.center());
        FullscreenHandler.attach(`btn-svg-fullscreen-${id}`, `svg-container-${id}`);
        DownloadHandler.attachSvg(`btn-svg-download-${id}`, svg, title);
    }

    _prefixInternalIds(svgElement, prefix) {
        const idMap = {};

        svgElement.querySelectorAll("[id]").forEach((el) => {
            const newId = `${prefix}-${el.id}`;
            idMap[el.id] = newId;
            el.id = newId;
        });

        const rewriteRefs = (value) =>
            Object.entries(idMap).reduce(
                (v, [oldId, newId]) =>
                    v
                        .replace(new RegExp(`url\\(#${oldId}\\)`, "g"), `url(#${newId})`)
                        .replace(new RegExp(`#${oldId}\\b`, "g"), `#${newId}`),
                value
            );

        svgElement.querySelectorAll("*").forEach((el) => {
            ["href", "xlink:href", "filter", "clip-path", "mask", "style"].forEach((attr) => {
                if (el.hasAttribute(attr)) el.setAttribute(attr, rewriteRefs(el.getAttribute(attr)));
            });
        });

        svgElement.querySelectorAll("style").forEach((styleEl) => {
            styleEl.textContent = rewriteRefs(styleEl.textContent);
        });
    }

    _resolveRelativeUrls(svgElement, basePath) {
        const toAbsolute = (value) =>
            value.replace(/url\((['"]?)([^"')]+)\1\)/g, (match, quote, url) => {
                if (/^(?:data:|https?:|#|\/)/i.test(url)) return match;
                return `url("${new URL(url, basePath).href}")`;
            });

        svgElement.querySelectorAll("style").forEach((el) => {
            el.textContent = toAbsolute(el.textContent);
        });

        svgElement.querySelectorAll("[style]").forEach((el) => {
            el.setAttribute("style", toAbsolute(el.getAttribute("style")));
        });

        svgElement.querySelectorAll("[href], [*|href]").forEach((el) => {
            ["href", "xlink:href"].forEach((attr) => {
                const val = el.getAttribute(attr);
                if (val && !/^(?:#|data:|https?:|\/)/i.test(val)) {
                    el.setAttribute(attr, new URL(val, basePath).href);
                }
            });
        });
    }

    _generateId(prefix) {
        return `${prefix}-${Math.random().toString(36).substr(2, 9)}`;
    }

    _renderError(rootElement, message) {
        rootElement.innerHTML = `<p style="color: red;">${message}</p>`;
        console.error(`SvgEmbed: ${message}`);
    }
}

// =============================================================================
// MÓDULO: EMBED DE IMAGEM
// =============================================================================

class ImageEmbed {
    async create(rootElement) {
        if (!rootElement || rootElement.dataset.embedLoaded) return;
        rootElement.dataset.embedLoaded = "true";

        const imagePath = rootElement.dataset.imagePath;
        const title = rootElement.dataset.title || "Imagem";

        if (!imagePath) {
            this._renderError(rootElement, "Atributo data-image-path não definido.");
            return;
        }

        const id = `image-${Math.random().toString(36).substr(2, 9)}`;
        rootElement.innerHTML = EmbedUIBuilder.buildHtml(id, title, "image");

        const img = this._createImageElement(id, imagePath);
        const canvas = document.getElementById(`canvas-area-${id}`);
        canvas.appendChild(img);

        try {
            await this._waitForImageLoad(img);
        } catch {
            rootElement.innerHTML = `<p style="color: red;">Erro ao carregar a imagem: ${imagePath}</p>`;
            return;
        }

        const controller = new ImagePanZoom(
            img,
            canvas,
            document.getElementById(`image-zoom-label-${id}`),
            null
        );

        this._attachControls(id, controller, img, title, imagePath);
    }

    // ---------------------------------------------------------------------------
    // Métodos privados
    // ---------------------------------------------------------------------------

    _createImageElement(id, src) {
        return Object.assign(document.createElement("img"), {
            src,
            id: `img-${id}`,
            style: Object.assign(document.createElement("img").style, {
                objectFit: "contain",
                transformOrigin: "center",
                willChange: "transform",
                transition: "transform 0.05s ease-out",
            }),
        });
    }

    _waitForImageLoad(img) {
        return new Promise((resolve, reject) => {
            img.onload = resolve;
            img.onerror = reject;
        });
    }

    _attachControls(id, controller, img, title, imagePath) {
        attachIconButton(`btn-image-zoom-in-${id}`, () => controller.applyZoom(ZOOM.STEP));
        attachIconButton(`btn-image-zoom-out-${id}`, () => controller.applyZoom(-ZOOM.STEP));
        attachIconButton(`btn-image-reset-${id}`, () => controller.reset());
        attachIconButton(`btn-image-center-${id}`, () => controller.center());
        FullscreenHandler.attach(`btn-image-fullscreen-${id}`, `image-container-${id}`);
        DownloadHandler.attachImage(`btn-image-download-${id}`, img, title, imagePath);
    }

    _renderError(rootElement, message) {
        rootElement.innerHTML = `<p style="color: red;">${message}</p>`;
        console.error(`ImageEmbed: ${message}`);
    }
}

// =============================================================================
// BOOTSTRAP
// =============================================================================

class EmbedManager {
    constructor() {
        this.svgEmbed = new SvgEmbed();
        this.imageEmbed = new ImageEmbed();
    }

    initializeAll() {
        document.querySelectorAll(".svg-embed-container").forEach((el) => this.svgEmbed.create(el));
        document.querySelectorAll(".image-embed-container").forEach((el) => this.imageEmbed.create(el));
    }
}

function bootstrap() {
    new EmbedManager().initializeAll();
}

document.addEventListener("DOMContentLoaded", bootstrap);
new MutationObserver(bootstrap).observe(document.body, { childList: true, subtree: true });