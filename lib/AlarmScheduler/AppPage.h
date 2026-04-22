#pragma once

const char kAppPageHtml[] = R"HTML(
<!doctype html>
<html lang='fr'>
<head>
    <meta charset='utf-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, viewport-fit=cover'>
    <meta name='apple-mobile-web-app-capable' content='yes'>
    <meta name='apple-mobile-web-app-status-bar-style' content='black-translucent'>
    <meta name='theme-color' content='#141218'>
    <title>PicoWake</title>
    <style>
        :root {
            --md-bg: #141218; --md-surface: #211f26; --md-surface-variant: #49454f;
            --md-primary: #d0bcff; --md-on-primary: #381e72; --md-secondary: #ccc2dc;
            --md-error: #ffb4ab; --md-text: #e6e1e5; --md-text-muted: #cac4d0;
            --md-outline: #938f99; --nav-height: 80px;
        }
        
        * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
        body {
            margin: 0; font-family: Roboto, system-ui, sans-serif;
            background: var(--md-bg); color: var(--md-text);
            user-select: none; -webkit-user-select: none; overscroll-behavior-y: none;
            padding-bottom: calc(var(--nav-height) + env(safe-area-inset-bottom));
        }

        header {
            position: sticky; top: 0; z-index: 10; background: var(--md-bg);
            padding: calc(16px + env(safe-area-inset-top)) 16px 16px;
            display: flex; align-items: center;
        }
        header h1 { font-size: 1.3rem; margin: 0; font-weight: 500; }

        .view { display: none; padding: 0 16px 16px; animation: fade .2s ease; }
        .view.active { display: block; }
        @keyframes fade { from { opacity: 0; transform: translateY(5px); } to { opacity: 1; transform: translateY(0); } }

        .card { background: var(--md-surface); border-radius: 16px; padding: 16px; margin-bottom: 16px; }
        h2 { font-size: 1.1rem; margin: 0 0 16px; font-weight: 500; color: var(--md-secondary); }
        label { font-size: .85rem; color: var(--md-text-muted); display: block; margin-bottom: 6px; }

        input[type='time'], input[type='text'] {
            width: 100%; height: 56px; padding: 0 16px;
            border: 1px solid var(--md-outline); border-radius: 4px 4px 0 0;
            background: var(--md-surface-variant); color: var(--md-text); 
            font-size: 1rem; border-bottom: 2px solid var(--md-text-muted);
            outline: none; margin-bottom: 16px;
        }
        input[type='time']::-webkit-calendar-picker-indicator { filter: invert(1); opacity: 0.7; }
        input:focus { border-bottom-color: var(--md-primary); }

        button {
            height: 40px; border: 0; border-radius: 20px; padding: 0 24px;
            font-weight: 500; font-size: .9rem; background: var(--md-primary); 
            color: var(--md-on-primary); cursor: pointer; transition: opacity .2s; outline: none;
        }
        button:active { opacity: 0.8; }
        button.text-btn { background: transparent; color: var(--md-primary); padding: 0 12px; }
        button:disabled { background: var(--md-surface-variant); color: rgba(255,255,255,0.3); }

        button.icon-btn { 
            width: 44px; height: 44px; padding: 0; border-radius: 50%;
            display: flex; align-items: center; justify-content: center;
            background: transparent; color: var(--md-text-muted);
        }
        button.icon-btn:active { background: rgba(255,255,255,0.1); }
        button.icon-btn.danger { color: var(--md-error); }
        button.icon-btn.danger:active { background: rgba(255,180,171,0.1); }

        .switch { position: relative; display: inline-block; width: 52px; height: 32px; }
        .switch input { opacity: 0; width: 0; height: 0; }
        .slider-toggle { 
            position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; 
            background-color: var(--md-surface-variant); border: 2px solid var(--md-outline);
            transition: .2s; border-radius: 32px; 
        }
        .slider-toggle:before { 
            position: absolute; content: ""; height: 20px; width: 20px; 
            left: 4px; bottom: 4px; background-color: var(--md-text-muted); 
            transition: .2s; border-radius: 50%; 
        }
        input:checked + .slider-toggle { background-color: var(--md-primary); border-color: var(--md-primary); }
        input:checked + .slider-toggle:before { transform: translateX(20px); background-color: var(--md-on-primary); width: 24px; height: 24px; bottom: 2px; }

        .fab {
            position: fixed; bottom: calc(var(--nav-height) + env(safe-area-inset-bottom) + 16px);
            right: 16px; z-index: 20; width: 56px; height: 56px; border-radius: 16px;
            background: var(--md-primary); color: var(--md-on-primary);
            display: flex; align-items: center; justify-content: center;
            box-shadow: 0 4px 8px rgba(0,0,0,0.3); padding: 0; display: none;
        }
        .fab svg { width: 24px; height: 24px; fill: currentColor; }

        nav {
            position: fixed; bottom: 0; left: 0; right: 0;
            height: calc(var(--nav-height) + env(safe-area-inset-bottom));
            background: var(--md-surface); z-index: 30; display: flex; 
            padding-bottom: env(safe-area-inset-bottom); border-top: 1px solid rgba(255,255,255,0.05);
        }
        .nav-item {
            flex: 1; display: flex; flex-direction: column; align-items: center; justify-content: center;
            background: transparent; color: var(--md-text-muted);
            border-radius: 0; padding: 0; height: var(--nav-height); gap: 4px;
        }
        .nav-item span { font-size: 0.75rem; font-weight: 500; }
        .nav-item .icon-wrapper {
            width: 64px; height: 32px; border-radius: 16px;
            display: flex; align-items: center; justify-content: center; transition: background .2s;
        }
        .nav-item svg { width: 24px; height: 24px; fill: currentColor; }
        .nav-item.active { color: var(--md-text); }
        .nav-item.active .icon-wrapper { background: rgba(208, 188, 255, 0.2); color: var(--md-primary); }

        .list-item { padding: 16px; margin-bottom: 8px; background: var(--md-surface); border-radius: 16px; }
        .alarm-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; }
        .time { font-size: 2.2rem; font-weight: 400; line-height: 1; color: var(--md-text); transition: opacity 0.3s; }
        .meta { font-size: .85rem; color: var(--md-text-muted); margin-top: 4px; transition: opacity 0.3s; }
        .alarm-actions { display: flex; gap: 16px; justify-content: space-between; align-items: center; border-top: 1px solid rgba(255,255,255,0.05); padding-top: 12px;}
        
        .days { display: flex; justify-content: space-between; margin-bottom: 24px; }
        .day {
            width: 36px; height: 36px; border-radius: 18px; display: flex; align-items: center; justify-content: center;
            font-size: .85rem; border: 1px solid var(--md-outline); color: var(--md-text-muted);
        }
        .day input { display: none; }
        .day:has(input:checked) { background: var(--md-primary); border-color: var(--md-primary); color: var(--md-on-primary); font-weight: 600; }

        dialog {
            background: var(--md-surface); border: none; border-radius: 28px;
            padding: 24px; width: 90%; max-width: 400px; color: var(--md-text);
        }
        dialog::backdrop { background: rgba(0,0,0,0.6); backdrop-filter: blur(2px); }
        .dialog-actions { display: flex; justify-content: flex-end; gap: 8px; margin-top: 16px; }

        .slider { width: 100%; height: 40px; accent-color: var(--md-primary); }
        .night-inputs { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; }
        .settings-actions { display: flex; gap: 8px; flex-wrap: wrap; }
        .settings-actions button { min-width: 160px; }
        .hint { font-size: .85rem; color: var(--md-text-muted); margin-top: 10px; }
        .hint.error { color: var(--md-error); }
    </style>
</head>
<body>
    <header><h1>PicoWake</h1></header>

    <main>
        <section id="tab-alarms" class="view active">
            <div id="list" style="padding-bottom: 80px;"></div>
        </section>

        <section id="tab-night" class="view">
            <div style="font-size:.85rem; color:var(--md-text-muted); margin:0 4px 16px;">Plages horaires où l'écran sera assombri.</div>
            <div id="nightRows" style="display:flex; flex-direction:column; padding-bottom: 80px;"></div>
        </section>

        <section id="tab-settings" class="view">
            <div class="list-item">
                <h2>Volume Principal</h2>
                <input id="vol" class="slider" type="range" min="0" max="100">
                <div id="volTxt" style="text-align:center; font-weight:500; color:var(--md-primary); margin-top:8px;"></div>
            </div>
            <div class="list-item">
                <h2>Alarmes JSON</h2>
                <div class="settings-actions">
                    <button type="button" id="downloadAlarms">Telecharger les alarmes</button>
                    <button type="button" id="uploadAlarms">Importer et remplacer</button>
                </div>
                <input id="uploadAlarmsInput" type="file" accept="application/json,.json" style="display:none;">
                <div id="jsonMsg" class="hint">L'import remplace les alarmes existantes par celles du fichier JSON.</div>
            </div>
        </section>
    </main>

    <button class="fab" id="fab-add"><svg viewBox="0 0 24 24"><path d="M19 13h-6v6h-2v-6H5v-2h6V5h2v6h6v2z"/></svg></button>

    <nav id="bottom-nav">
        <button class="nav-item active" data-tab="tab-alarms">
            <div class="icon-wrapper"><svg viewBox="0 0 24 24"><path d="M11.99 2C6.47 2 2 6.48 2 12s4.47 10 9.99 10C17.52 22 22 17.52 22 12S17.52 2 11.99 2zM12 20c-4.42 0-8-3.58-8-8s3.58-8 8-8 8 3.58 8 8-3.58 8-8 8zm.5-13H11v6l5.25 3.15.75-1.23-4.5-2.67z"/></svg></div>
            <span>Réveils</span>
        </button>
        <button class="nav-item" data-tab="tab-night">
            <div class="icon-wrapper"><svg viewBox="0 0 24 24"><path d="M12 3c-4.97 0-9 4.03-9 9s4.03 9 9 9 9-4.03 9-9c0-.46-.04-.92-.1-1.36-.98 1.37-2.58 2.26-4.4 2.26-2.98 0-5.4-2.42-5.4-5.4 0-1.81.89-3.42 2.26-4.4-.44-.06-.9-.1-1.36-.1z"/></svg></div>
            <span>Nuit</span>
        </button>
        <button class="nav-item" data-tab="tab-settings">
            <div class="icon-wrapper"><svg viewBox="0 0 24 24"><path d="M19.14 12.94c.04-.31.06-.63.06-.94s-.02-.63-.06-.94l2.03-1.58a.5.5 0 0 0 .12-.64l-1.92-3.32a.5.5 0 0 0-.6-.22l-2.39.96a7.03 7.03 0 0 0-1.63-.94l-.36-2.54a.5.5 0 0 0-.49-.42h-3.84a.5.5 0 0 0-.49.42l-.36 2.54c-.58.23-1.13.54-1.63.94l-2.39-.96a.5.5 0 0 0-.6.22L2.7 8.84a.5.5 0 0 0 .12.64l2.03 1.58c-.04.31-.06.63-.06.94s.02.63.06.94L2.82 14.5a.5.5 0 0 0-.12.64l1.92 3.32c.13.23.39.32.6.22l2.39-.96c.5.4 1.05.72 1.63.94l.36 2.54c.04.24.25.42.49.42h3.84c.24 0 .45-.18.49-.42l.36-2.54c.58-.23 1.13-.54 1.63-.94l2.39.96c.22.1.47.01.6-.22l1.92-3.32a.5.5 0 0 0-.12-.64l-2.03-1.56zM12 15.6A3.6 3.6 0 1 1 12 8.4a3.6 3.6 0 0 1 0 7.2z"/></svg></div>
            <span>Parametres</span>
        </button>
    </nav>

    <dialog id="addModal">
        <h2 style="margin-top:0;">Nouveau Réveil</h2>
        <label>Heure</label><input id="time" type="time" value="07:00">
        <label>Nom (optionnel)</label><input id="label" type="text" placeholder="Ex: Travail">
        <label>Jours de répétition</label>
        <div class="days" id="days"></div>
        <div class="dialog-actions">
            <button class="text-btn" id="addCancel">Annuler</button>
            <button id="addSave">Ajouter</button>
        </div>
        <div id="addMsg" style="margin-top:8px; font-size:.85rem; color:var(--md-error);"></div>
    </dialog>

    <script>
        const fab = document.getElementById('fab-add');
        const modal = document.getElementById('addModal');
        const views = document.querySelectorAll('.view');
        const navItems = document.querySelectorAll('.nav-item');

        function switchTab(tabId) {
            views.forEach(v => v.classList.remove('active'));
            navItems.forEach(n => n.classList.remove('active'));
            document.getElementById(tabId).classList.add('active');
            document.querySelector(`[data-tab="${tabId}"]`).classList.add('active');
            fab.style.display = (tabId === 'tab-alarms') ? 'flex' : 'none';
        }
        navItems.forEach(btn => btn.addEventListener('click', () => switchTab(btn.dataset.tab)));
        fab.addEventListener('click', () => modal.showModal());
        document.getElementById('addCancel').addEventListener('click', () => modal.close());
        switchTab('tab-alarms');

        const dayNames = ['D','L','M','M','J','V','S']; 
        const dayNamesLong = ['Dimanche','Lundi','Mardi','Mercredi','Jeudi','Vendredi','Samedi'];
        
        // Ordre français : du Lundi (1) au Dimanche (0)
        const orderedDays = [1, 2, 3, 4, 5, 6, 0]; 
        
        const state = {alarms:[], volume:70, nightWindows:Array.from({length:7},()=>({startMinute:1260,endMinute:420}))};
        let editNightDay = -1; // -1 = aucun jour en cours d'édition

        function setJsonMessage(text, isError = false) {
            const msg = document.getElementById('jsonMsg');
            msg.textContent = text;
            msg.classList.toggle('error', Boolean(isError));
        }

        function sanitizeAlarm(raw) {
            if (!raw || typeof raw !== 'object') return null;
            const hour = Number(raw.hour);
            const minute = Number(raw.minute);
            if (!Number.isInteger(hour) || hour < 0 || hour > 23) return null;
            if (!Number.isInteger(minute) || minute < 0 || minute > 59) return null;
            return {
                hour,
                minute,
                daysMask: Number.isInteger(Number(raw.daysMask)) ? Number(raw.daysMask) : 0,
                label: typeof raw.label === 'string' ? raw.label : '',
                enabled: raw.enabled !== false
            };
        }

        async function replaceAlarmsFromJson(alarms) {
            const safeAlarms = alarms.map(sanitizeAlarm).filter(Boolean);
            if (!safeAlarms.length) throw new Error('Aucune alarme valide dans le fichier JSON.');

            await refresh();
            for (const a of state.alarms) {
                await api('/api/alarms?id=' + a.id, { method: 'DELETE' });
            }
            for (const alarm of safeAlarms) {
                await api('/api/alarms', { method: 'POST', body: JSON.stringify(alarm) });
            }
            await refresh();
        }

        function exportAlarmsJson() {
            const payload = {
                version: 1,
                exportedAt: new Date().toISOString(),
                alarms: state.alarms.map(a => ({
                    hour: Number(a.hour) || 0,
                    minute: Number(a.minute) || 0,
                    daysMask: Number(a.daysMask) || 0,
                    label: a.label || '',
                    enabled: a.enabled !== false
                }))
            };
            const blob = new Blob([JSON.stringify(payload, null, 2)], { type: 'application/json' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = 'picowake-alarms.json';
            document.body.appendChild(a);
            a.click();
            a.remove();
            URL.revokeObjectURL(url);
            setJsonMessage('Export JSON termine.');
        }

        // Init Jours de la modale (dans le bon ordre L->D)
        orderedDays.forEach(i => {
            const l = document.createElement('label'); 
            l.className = 'day';
            l.innerHTML = `<input type='checkbox' data-day='${i}' ${(i>=1&&i<=5)?'checked':''}>${dayNames[i]}`;
            document.getElementById('days').appendChild(l);
        });

        async function api(path,opt={}){
            const r=await fetch(path,{headers:{'Content-Type':'application/json'},...opt});
            if(!r.ok) throw new Error(await r.text());
            return r.json();
        }

        function m2t(m){ m=(m||0)%1440; return `${String(Math.floor(m/60)).padStart(2,'0')}h${String(m%60).padStart(2,'0')}`; }
        function m2tInput(m){ m=(m||0)%1440; return `${String(Math.floor(m/60)).padStart(2,'0')}:${String(m%60).padStart(2,'0')}`; }
        function t2m(t){ t=(t||'00:00').split(':').map(Number); return Math.max(0,Math.min(1439,(t[0]||0)*60+(t[1]||0))); }
        function mask2t(m){
            const o=[]; 
            // Affiche les jours activés dans le bon ordre Lundi->Dimanche
            orderedDays.forEach(i => {
                if(m&(1<<i)) o.push(dayNamesLong[i].substring(0,3));
            });
            return o.length ? o.join(', ') : 'Aucun jour';
        }

        function render(){
            // Render Volume
            document.getElementById('vol').value=state.volume;
            document.getElementById('volTxt').textContent=`${state.volume}%`;
            
            // Render Nuit (List Item / Edition Inline - dans le bon ordre L->D)
            const nRows = document.getElementById('nightRows');
            nRows.innerHTML = '';
            orderedDays.forEach(i => {
                const w = state.nightWindows[i];
                const el = document.createElement('div');
                el.className = 'list-item';
                
                if(editNightDay === i) {
                    // Mode Edition
                    el.innerHTML = `
                        <div style="font-weight:500; margin-bottom:12px; color:var(--md-primary);">${dayNamesLong[i]}</div>
                        <div class="night-inputs">
                            <div><label>Début</label><input id="ns${i}" type="time" value="${m2tInput(w.startMinute)}"></div>
                            <div><label>Fin</label><input id="ne${i}" type="time" value="${m2tInput(w.endMinute)}"></div>
                        </div>
                        <div class="dialog-actions" style="margin-top:0;">
                            <button class="text-btn" data-cancel-night="1">Annuler</button>
                            <button data-save-night="${i}">Valider</button>
                        </div>
                    `;
                } else {
                    // Mode Lecture
                    el.innerHTML = `
                        <div style="display:flex; justify-content:space-between; align-items:center;">
                            <div>
                                <div style="font-weight:500; color:var(--md-text);">${dayNamesLong[i]}</div>
                                <div class="meta" style="margin-top:2px;">${m2t(w.startMinute)} - ${m2t(w.endMinute)}</div>
                            </div>
                            <button class="icon-btn" data-edit-night="${i}">
                                <svg viewBox="0 0 24 24" width="20" height="20" style="pointer-events:none"><path fill="currentColor" d="M3 17.25V21h3.75L17.81 9.94l-3.75-3.75L3 17.25zM20.71 7.04c.39-.39.39-1.02 0-1.41l-2.34-2.34c-.39-.39-1.02-.39-1.41 0l-1.83 1.83 3.75 3.75 1.83-1.83z"/></svg>
                            </button>
                        </div>
                    `;
                }
                nRows.appendChild(el);
            });
            
            // Render Alarmes
            const list=document.getElementById('list'); list.innerHTML='';
            if(state.alarms.length === 0) {
                list.innerHTML = "<div style='text-align:center; color:var(--md-text-muted); margin-top:40px;'>Aucun réveil programmé</div>";
                return;
            }
            
            state.alarms.sort((a,b) => (a.hour*60+a.minute) - (b.hour*60+b.minute)).forEach(a=>{
                const el=document.createElement('div'); el.className='list-item';
                el.innerHTML=`
                    <div class='alarm-header'>
                        <div>
                            <div class='time' style='opacity:${a.enabled?1:0.4}'>${String(a.hour).padStart(2,'0')}:${String(a.minute).padStart(2,'0')}</div>
                            <div class='meta' style='opacity:${a.enabled?1:0.5}'>${a.label||'Réveil'} • ${mask2t(a.daysMask)}</div>
                        </div>
                    </div>
                    <div class='alarm-actions'>
                        <button class='icon-btn danger' data-del='${a.id}'>
                            <svg viewBox="0 0 24 24" width="24" height="24" style="pointer-events:none"><path fill="currentColor" d="M6 19c0 1.1.9 2 2 2h8c1.1 0 2-.9 2-2V7H6v12zM19 4h-3.5l-1-1h-5l-1 1H5v2h14V4z"/></svg>
                        </button>
                        <label class="switch">
                            <input type="checkbox" ${a.enabled ? 'checked' : ''} data-toggle="${a.id}">
                            <span class="slider-toggle"></span>
                        </label>
                    </div>`;
                list.appendChild(el);
            });
        }

        async function refresh(){
            try {
                const s=await api('/api/state');
                state.alarms=s.alarms||[]; state.volume=s.volume||70; 
                state.nightWindows=Array.isArray(s.nightWindows) ? s.nightWindows : state.nightWindows;
                // On évite de rafraichir si l'utilisateur est en train d'écrire dans la modale
                if (!modal.open) render();
            } catch(e) {}
        }

        // Actions globales
        document.getElementById('addSave').onclick=async(e)=>{
            e.preventDefault();
            const btn = document.getElementById('addSave');
            const msg = document.getElementById('addMsg');
            msg.textContent = '';

            const rawTime = document.getElementById('time').value || '07:00';
            const parts = rawTime.split(':').map(Number);
            const h = Number.isFinite(parts[0]) ? parts[0] : 7;
            const m = Number.isFinite(parts[1]) ? parts[1] : 0;

            let mask = 0;
            document.querySelectorAll('#days input:checked').forEach(ch => mask |= (1 << Number(ch.dataset.day)));

            btn.disabled = true;
            try {
                await api('/api/alarms', {
                    method:'POST',
                    body:JSON.stringify({
                        hour:h,
                        minute:m,
                        daysMask:mask,
                        label:document.getElementById('label').value || '',
                        enabled:true
                    })
                });

                document.getElementById('label').value = '';
                if (typeof modal.close === 'function') {
                    modal.close();
                }
                await refresh();
            } catch (err) {
                msg.textContent = 'Impossible d\'ajouter le reveil';
            } finally {
                btn.disabled = false;
            }
        };

        document.getElementById('vol').onchange=async(e)=>{
            state.volume=Number(e.target.value);
            await api('/api/volume',{method:'POST',body:JSON.stringify({value:state.volume})});
        };
        document.getElementById('vol').oninput=(e)=> document.getElementById('volTxt').textContent=`${e.target.value}%`;

        document.getElementById('downloadAlarms').addEventListener('click', () => {
            try {
                exportAlarmsJson();
            } catch (err) {
                setJsonMessage('Export impossible.', true);
            }
        });

        document.getElementById('uploadAlarms').addEventListener('click', () => {
            document.getElementById('uploadAlarmsInput').click();
        });

        document.getElementById('uploadAlarmsInput').addEventListener('change', async (e) => {
            const file = e.target.files && e.target.files[0];
            if (!file) return;
            try {
                const text = await file.text();
                const parsed = JSON.parse(text);
                const alarms = Array.isArray(parsed) ? parsed : parsed.alarms;
                if (!Array.isArray(alarms)) {
                    throw new Error('Format JSON invalide');
                }
                await replaceAlarmsFromJson(alarms);
                setJsonMessage('Import termine: les alarmes ont ete remplacees.');
            } catch (err) {
                setJsonMessage('Import impossible: JSON invalide ou alarme non valide.', true);
            } finally {
                e.target.value = '';
            }
        });

        // Event delegation Menu Nuit
        document.getElementById('nightRows').addEventListener('click', async (e) => {
            if(e.target.closest('[data-edit-night]')) { editNightDay = Number(e.target.closest('[data-edit-night]').dataset.editNight); render(); }
            if(e.target.closest('[data-cancel-night]')) { editNightDay = -1; render(); }
            
            const btnSave = e.target.closest('[data-save-night]');
            if(btnSave) {
                const day = Number(btnSave.dataset.saveNight);
                const s = t2m(document.getElementById(`ns${day}`).value);
                const e_val = t2m(document.getElementById(`ne${day}`).value);
                
                state.nightWindows[day] = {startMinute: s, endMinute: e_val};
                editNightDay = -1;
                render(); // UI Optimiste
                
                try {
                    await api('/api/night-window', { method:'POST', body:JSON.stringify({windows: [{day, startMinute: s, endMinute: e_val}]}) });
                } catch(err) { console.error(err); }
            }
        });

        // Event delegation Alarmes
        document.getElementById('list').addEventListener('click', async (e) => {
            const btn = e.target.closest('[data-del]');
            if (btn) {
                await api('/api/alarms?id=' + btn.dataset.del, {method: 'DELETE'});
                await refresh();
            }
        });

        document.getElementById('list').addEventListener('change', async (e) => {
            if (e.target.dataset.toggle) {
                const id = Number(e.target.dataset.toggle);
                await api('/api/enabled', {method: 'POST', body: JSON.stringify({id, enabled: e.target.checked})});
                await refresh();
            }
        });

        refresh(); setInterval(refresh, 3000);
    </script>
</body>
</html>
)HTML";