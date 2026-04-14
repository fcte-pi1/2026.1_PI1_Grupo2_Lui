const MazeAnimation = (function() {
    let animationFrameId = null;
    let resizeHandler = null;

    function init() {
        const canvas = document.getElementById('bgCanvas');
        if (!canvas) return;

        const ctx = canvas.getContext('2d');

        // Configurações de grade e dimensão das células baseadas no viewport
        let cellSize = 50; 
        let cols, rows;
        let offsetX, offsetY;
        let grid = [];
        let path = [];
        let state = 'INIT'; 
        
        // Paleta de cores centralizada (Racing/Engineering theme)
        const cores = {
            fundoRadial1: '#1b0b3b', 
            fundoRadial2: '#090214', 
            parede: '#4e2a84',       
            ratoCorpo: '#9e9e9e',    
            ratoOrelhas: '#dcb4b4',  
            ratoOlhos: '#000000',
            ratoFocinho: '#dcb4b4',  
            queijo: '#fee440',       
            queijoFuros: '#f5b700',
            queijoSombra: '#d00000'
        };

        // Estado interno do agente (MicroMouse)
        let ratinho = {
            x: 0, y: 0, px: 0, py: 0, angulo: 0,
            pathIndex: 0, progresso: 0, velocidade: 2.5, cicloAnima: 0
        };

        let particulas = [];

        // Redimensionamento dinâmico e recalculação da grade
        function resize() {
            if (!document.getElementById('bgCanvas')) return;
            canvas.width = window.innerWidth;
            canvas.height = window.innerHeight;
            cellSize = Math.max(40, Math.min(canvas.width, canvas.height) / 20);
            cols = Math.floor(canvas.width / cellSize) - 2;
            rows = Math.floor(canvas.height / cellSize) - 2;
            if(cols < 5) cols = 5;
            if(rows < 5) rows = 5;
            offsetX = (canvas.width - cols * cellSize) / 2;
            offsetY = (canvas.height - rows * cellSize) / 2;
            iniciarNovaRodada();
        }

        if (resizeHandler) window.removeEventListener('resize', resizeHandler);
        resizeHandler = resize;
        window.addEventListener('resize', resizeHandler);

        /**
         * Gera um labirinto aleatório usando algoritmo de Busca em Profundidade (DFS)
         * com backtracking para garantir que todas as áreas sejam acessíveis.
         */
        function criarLabirinto() {
            grid = [];
            for (let i = 0; i < cols; i++) {
                grid[i] = [];
                for (let j = 0; j < rows; j++) {
                    grid[i][j] = { 
                        x: i, y: j, visitado: false, 
                        paredes: { topo: true, direita: true, baixo: true, esquerda: true } 
                    };
                }
            }
            let pilha = [];
            let atual = grid[0][0];
            atual.visitado = true;
            pilha.push(atual);
            while (pilha.length > 0) {
                let vizinhosNaoVisitados = [];
                let x = atual.x; let y = atual.y;
                if (y > 0 && !grid[x][y-1].visitado) vizinhosNaoVisitados.push({celula: grid[x][y-1], dir: 'topo'});
                if (x < cols-1 && !grid[x+1][y].visitado) vizinhosNaoVisitados.push({celula: grid[x+1][y], dir: 'direita'});
                if (y < rows-1 && !grid[x][y+1].visitado) vizinhosNaoVisitados.push({celula: grid[x][y+1], dir: 'baixo'});
                if (x > 0 && !grid[x-1][y].visitado) vizinhosNaoVisitados.push({celula: grid[x-1][y], dir: 'esquerda'});
                if (vizinhosNaoVisitados.length > 0) {
                    let proximo = vizinhosNaoVisitados[Math.floor(Math.random() * vizinhosNaoVisitados.length)];
                    pilha.push(atual);
                    if (proximo.dir === 'topo') { atual.paredes.topo = false; proximo.celula.paredes.baixo = false; }
                    if (proximo.dir === 'direita') { atual.paredes.direita = false; proximo.celula.paredes.esquerda = false; }
                    if (proximo.dir === 'baixo') { atual.paredes.baixo = false; proximo.celula.paredes.topo = false; }
                    if (proximo.dir === 'esquerda') { atual.paredes.esquerda = false; proximo.celula.paredes.direita = false; }
                    atual = proximo.celula; atual.visitado = true;
                } else { atual = pilha.pop(); }
            }
        }

        /**
         * Encontra o caminho mais curto entre dois pontos usando BFS (Busca em Largura).
         * Retorna um array de coordenadas das células.
         */
        function encontrarCaminho(inicio, fim) {
            let fila = [{ celula: inicio, caminho: [inicio] }];
            let visitados = new Set();
            visitados.add(`${inicio.x},${inicio.y}`);
            while (fila.length > 0) {
                let { celula, caminho } = fila.shift();
                if (celula.x === fim.x && celula.y === fim.y) return caminho;
                let vizinhos = [];
                if (!celula.paredes.topo && celula.y > 0) vizinhos.push(grid[celula.x][celula.y-1]);
                if (!celula.paredes.direita && celula.x < cols - 1) vizinhos.push(grid[celula.x+1][celula.y]);
                if (!celula.paredes.baixo && celula.y < rows - 1) vizinhos.push(grid[celula.x][celula.y+1]);
                if (!celula.paredes.esquerda && celula.x > 0) vizinhos.push(grid[celula.x-1][celula.y]);
                for (let v of vizinhos) {
                    let chave = `${v.x},${v.y}`;
                    if (!visitados.has(chave)) {
                        visitados.add(chave);
                        fila.push({ celula: v, caminho: [...caminho, v] });
                    }
                }
            }
            return [];
        }

        // Reseta o estado para uma nova simulação
        function iniciarNovaRodada() {
            criarLabirinto();
            let startY = Math.floor(Math.random() * rows);
            let endY = Math.floor(Math.random() * rows);
            let inicio = grid[0][startY]; let fim = grid[cols-1][endY];
            inicio.paredes.esquerda = false; fim.paredes.direita = false;
            path = encontrarCaminho(inicio, fim);
            path.unshift({ x: inicio.x - 1, y: inicio.y });
            path.push({ x: fim.x + 1, y: fim.y });
            ratinho.pathIndex = 0; ratinho.progresso = 0;
            atualizarPosicaoRatinhoEmPixels(path[0].x, path[0].y);
            if(path.length > 1) {
                let dx = path[1].x - path[0].x; let dy = path[1].y - path[0].y;
                ratinho.angulo = Math.atan2(dy, dx);
            }
            particulas = []; state = 'ANIMATING';
        }

        // Converte coordenadas da grade (cols, rows) em coordenadas de pixel no canvas
        function obterPixels(cx, cy) {
            return { x: offsetX + cx * cellSize + cellSize / 2, y: offsetY + cy * cellSize + cellSize / 2 };
        }

        function atualizarPosicaoRatinhoEmPixels(cx, cy) {
            let p = obterPixels(cx, cy); ratinho.px = p.x; ratinho.py = p.y;
        }

        // Utilitários de interpolação matemática
        function lerp(a, b, t) { return a + (b - a) * t; }
        function lerpAngulo(a, b, t) {
            let diff = b - a;
            while (diff > Math.PI) diff -= Math.PI * 2;
            while (diff < -Math.PI) diff += Math.PI * 2;
            return a + diff * t;
        }

        // Renderização dos muros do labirinto
        function desenharLabirinto() {
            ctx.shadowColor = 'rgba(157, 78, 221, 0.4)';
            ctx.shadowBlur = 10;
            ctx.strokeStyle = cores.parede;
            ctx.lineWidth = Math.max(4, cellSize * 0.12);
            ctx.beginPath();
            for (let i = 0; i < cols; i++) {
                for (let j = 0; j < rows; j++) {
                    let celula = grid[i][j];
                    let x = offsetX + i * cellSize; let y = offsetY + j * cellSize;
                    if (celula.paredes.topo) { ctx.moveTo(x, y); ctx.lineTo(x + cellSize, y); }
                    if (celula.paredes.direita) { ctx.moveTo(x + cellSize, y); ctx.lineTo(x + cellSize, y + cellSize); }
                    if (celula.paredes.baixo) { ctx.moveTo(x, y + cellSize); ctx.lineTo(x + cellSize, y + cellSize); }
                    if (celula.paredes.esquerda) { ctx.moveTo(x, y); ctx.lineTo(x, y + cellSize); }
                }
            }
            ctx.stroke();
            ctx.shadowColor = 'transparent';
        }

        // Renderização do objetivo final da simulação
        function desenharQueijo(cx, cy, tempo) {
            let p = obterPixels(cx, cy); let tamanho = cellSize * 0.45;
            ctx.save();
            ctx.translate(p.x, p.y);
            if (state === 'ANIMATING') ctx.translate(0, Math.sin(tempo * 0.004) * 4);
            ctx.shadowColor = 'rgba(255, 202, 58, 0.4)';
            ctx.shadowBlur = 20 + Math.sin(tempo * 0.005) * 10;
            ctx.fillStyle = cores.queijoSombra;
            ctx.beginPath(); ctx.moveTo(-tamanho/2, tamanho/2 + 3); ctx.lineTo(tamanho/2, tamanho/2 + 3); ctx.lineTo(tamanho/2, -tamanho/2 + 3); ctx.closePath(); ctx.fill();
            ctx.shadowColor = 'transparent'; 
            ctx.fillStyle = cores.queijo;
            ctx.beginPath(); ctx.moveTo(-tamanho/2, tamanho/2); ctx.lineTo(tamanho/2, tamanho/2); ctx.lineTo(tamanho/2, -tamanho/2); ctx.closePath(); ctx.fill();
            ctx.fillStyle = cores.queijoFuros;
            ctx.beginPath(); ctx.arc(-tamanho*0.1, tamanho*0.2, tamanho*0.12, 0, Math.PI*2); ctx.fill();
            ctx.beginPath(); ctx.arc(tamanho*0.2, tamanho*0.3, tamanho*0.08, 0, Math.PI*2); ctx.fill();
            ctx.beginPath(); ctx.arc(tamanho*0.3, -tamanho*0.1, tamanho*0.06, 0, Math.PI*2); ctx.fill();
            ctx.restore();
        }

        // Renderização do agente móvel baseado em curvas de Bezier
        function desenharRatinho(tempo) {
            ctx.save(); ctx.translate(ratinho.px, ratinho.py);
            ctx.shadowColor = 'rgba(0, 0, 0, 0.6)'; ctx.shadowBlur = 10; ctx.shadowOffsetX = 3; ctx.shadowOffsetY = 5;
            ctx.rotate(ratinho.angulo);
            let escala = cellSize / 50; ctx.scale(escala, escala);
            let andando = state === 'ANIMATING';
            let balanco = andando ? Math.sin(ratinho.cicloAnima * 0.5) : 0; 
            let patasAnimEsq = andando ? Math.sin(ratinho.cicloAnima) : 0;
            let patasAnimDir = andando ? Math.sin(ratinho.cicloAnima + Math.PI) : 0;
            ctx.beginPath(); ctx.moveTo(-12, 0); ctx.bezierCurveTo(-25, balanco * 15, -30, -balanco * 10, -40, Math.sin(tempo * 0.015) * 12);
            ctx.strokeStyle = cores.ratoOrelhas; ctx.lineWidth = 3; ctx.lineCap = 'round'; ctx.stroke();
            ctx.fillStyle = cores.ratoCorpo;
            ctx.beginPath(); ctx.ellipse(-6 + patasAnimEsq * 4, -10, 4, 3, 0, 0, Math.PI*2); ctx.fill();
            ctx.beginPath(); ctx.ellipse(-6 + patasAnimDir * 4, 10, 4, 3, 0, 0, Math.PI*2); ctx.fill();
            ctx.beginPath(); ctx.ellipse(6 + patasAnimDir * 4, -10, 3, 2, 0, 0, Math.PI*2); ctx.fill();
            ctx.beginPath(); ctx.ellipse(6 + patasAnimEsq * 4, 10, 3, 2, 0, 0, Math.PI*2); ctx.fill();
            ctx.fillStyle = cores.ratoCorpo; ctx.beginPath(); ctx.moveTo(-16, 0); ctx.bezierCurveTo(-16, -15, 6, -13, 16, 0); ctx.bezierCurveTo(6, 13, -16, 15, -16, 0); ctx.fill();
            ctx.shadowColor = 'transparent';
            let orelhaMovel = andando ? Math.cos(ratinho.cicloAnima * 0.5) * 0.5 : 0;
            ctx.fillStyle = cores.ratoCorpo; ctx.beginPath(); ctx.arc(1, -10 + orelhaMovel, 7.5, 0, Math.PI * 2); ctx.fill();
            ctx.fillStyle = cores.ratoOrelhas; ctx.beginPath(); ctx.arc(1, -10 + orelhaMovel, 4.5, 0, Math.PI * 2); ctx.fill();
            ctx.fillStyle = cores.ratoCorpo; ctx.beginPath(); ctx.arc(1, 10 - orelhaMovel, 7.5, 0, Math.PI * 2); ctx.fill();
            ctx.fillStyle = cores.ratoOrelhas; ctx.beginPath(); ctx.arc(1, 10 - orelhaMovel, 4.5, 0, Math.PI * 2); ctx.fill();
            ctx.fillStyle = cores.ratoFocinho; ctx.globalAlpha = 0.5; ctx.beginPath(); ctx.arc(10, -7, 2.5, 0, Math.PI * 2); ctx.fill(); ctx.beginPath(); ctx.arc(10, 7, 2.5, 0, Math.PI * 2); ctx.fill(); ctx.globalAlpha = 1.0;
            ctx.fillStyle = cores.ratoFocinho; ctx.beginPath(); ctx.arc(16.5, 0, 3, 0, Math.PI * 2); ctx.fill();
            ctx.fillStyle = cores.ratoOlhos; ctx.beginPath(); ctx.arc(11, -4.5, 2, 0, Math.PI * 2); ctx.fill(); ctx.beginPath(); ctx.arc(11, 4.5, 2, 0, Math.PI * 2); ctx.fill();
            ctx.fillStyle = '#ffffff'; ctx.beginPath(); ctx.arc(11.5, -5, 0.8, 0, Math.PI * 2); ctx.fill(); ctx.beginPath(); ctx.arc(11.5, 4, 0.8, 0, Math.PI * 2); ctx.fill();
            ctx.strokeStyle = 'rgba(0,0,0,0.3)'; ctx.lineWidth = 0.8;
            ctx.beginPath(); ctx.moveTo(15, -2); ctx.lineTo(23, -8); ctx.stroke(); ctx.beginPath(); ctx.moveTo(15, -1); ctx.lineTo(24, -4); ctx.stroke();
            ctx.beginPath(); ctx.moveTo(15, 2); ctx.lineTo(23, 8); ctx.stroke(); ctx.beginPath(); ctx.moveTo(15, 1); ctx.lineTo(24, 4); ctx.stroke();
            ctx.restore();
        }

        function desenharParticulas() {
            for (let p of particulas) {
                ctx.save(); ctx.translate(p.x, p.y); ctx.scale(p.escala, p.escala); ctx.globalAlpha = p.alpha;
                ctx.shadowColor = p.cor; ctx.shadowBlur = 15;
                ctx.fillStyle = p.cor; ctx.beginPath(); ctx.moveTo(0, 0); ctx.bezierCurveTo(0, -3, -5, -15, -15, -15); ctx.bezierCurveTo(-30, -15, -30, 2.5, -30, 2.5); ctx.bezierCurveTo(-30, 15, -10, 26, 0, 35); ctx.bezierCurveTo(10, 26, 30, 15, 30, 2.5); ctx.bezierCurveTo(30, 2.5, 30, -15, 15, -15); ctx.bezierCurveTo(5, -15, 0, -3, 0, 0); ctx.fill();
                ctx.restore();
            }
        }

        let ultimoTempo = 0;
        let temporizadorWin = 0;

        // Loop de animação principal (RequestAnimationFrame)
        function loop(tempoAtual) {
            if (!document.getElementById('bgCanvas')) {
                animationFrameId = null;
                return;
            }
            
            let deltaTime = (tempoAtual - ultimoTempo) / 1000;
            if (deltaTime > 0.1) deltaTime = 0.1; // Cap para evitar saltos bruscos se a aba for pausada
            ultimoTempo = tempoAtual;

            // Background dinâmico com gradiente radial
            let gradient = ctx.createRadialGradient(canvas.width / 2, canvas.height / 2, 0, canvas.width / 2, canvas.height / 2, Math.max(canvas.width, canvas.height));
            gradient.addColorStop(0, cores.fundoRadial1);
            gradient.addColorStop(1, cores.fundoRadial2);
            ctx.fillStyle = gradient; ctx.fillRect(0, 0, canvas.width, canvas.height);

            if (grid.length > 0) {
                desenharLabirinto();
                if (path.length > 0) {
                    let celulaFim = path[path.length - 1];
                    if (state !== 'WIN' || temporizadorWin < 1.0) desenharQueijo(celulaFim.x, celulaFim.y, tempoAtual);
                }
                desenharRatinho(tempoAtual);
            }
            desenharParticulas();

            // Lógica de movimentação em tempo real (Linear Interpolation)
            if (state === 'ANIMATING') {
                ratinho.progresso += ratinho.velocidade * deltaTime;
                ratinho.cicloAnima += ratinho.velocidade * deltaTime * 15;
                
                if (ratinho.progresso >= 1.0) {
                    ratinho.progresso = 0; ratinho.pathIndex++;
                    
                    // Condição de vitória: alvo alcançado
                    if (ratinho.pathIndex >= path.length - 1) {
                        state = 'WIN'; temporizadorWin = 0;
                        let coresCoracoes = ['#9d4edd', '#c77dff', '#7b2cbf', '#e0b1cb'];
                        for(let i=0; i<12; i++){
                            particulas.push({
                                x: ratinho.px, y: ratinho.py, vx: (Math.random() - 0.5) * 120, vy: -Math.random() * 120 - 40,
                                alpha: 1.0, escala: Math.random() * 0.3 + 0.2, cor: coresCoracoes[Math.floor(Math.random() * coresCoracoes.length)]
                            });
                        }
                    }
                }
                
                if (state === 'ANIMATING') {
                    let atual = path[ratinho.pathIndex]; let proximo = path[ratinho.pathIndex + 1];
                    let pAtual = obterPixels(atual.x, atual.y); let pProximo = obterPixels(proximo.x, proximo.y);
                    
                    // Movimento suave entre as células
                    ratinho.px = lerp(pAtual.x, pProximo.x, ratinho.progresso); 
                    ratinho.py = lerp(pAtual.y, pProximo.y, ratinho.progresso);
                    
                    // Rotação suave baseada no vetor de direção
                    let anguloAlvo = Math.atan2(pProximo.y - pAtual.y, pProximo.x - pAtual.x);
                    ratinho.angulo = lerpAngulo(ratinho.angulo, anguloAlvo, 0.15);
                }
            } 
            else if (state === 'WIN') {
                temporizadorWin += deltaTime;
                // Animação de partículas de vitória
                for (let i = particulas.length - 1; i >= 0; i--) {
                    let p = particulas[i]; p.x += p.vx * deltaTime; p.y += p.vy * deltaTime; p.alpha -= deltaTime * 0.8;
                    if (p.alpha <= 0) particulas.splice(i, 1);
                }
                if (temporizadorWin > 2.5) iniciarNovaRodada();
            }
            animationFrameId = requestAnimationFrame(loop);
        }

        resize();
        animationFrameId = requestAnimationFrame(loop);
    }

    return { init };
})();

/**
 * Ponto de entrada compatível com a navegação instantânea do MkDocs Material.
 * Garante que a animação reinicie sem duplicar instâncias.
 */
function bootstrapMazeAnimation() {
    const canvas = document.getElementById('bgCanvas');
    if (canvas && !canvas.dataset.mazeBgLoaded) {
        canvas.dataset.mazeBgLoaded = 'true';
        MazeAnimation.init();
    }
}

// Inicialização inicial
window.addEventListener('DOMContentLoaded', bootstrapMazeAnimation);

// Observador de mutação para triggers de navegação sem reload
const mazeObserver = new MutationObserver(bootstrapMazeAnimation);
mazeObserver.observe(document.body, { childList: true, subtree: true });
