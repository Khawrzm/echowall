"""ECHOWALL REST + WebSocket API."""

from __future__ import annotations
from fastapi import FastAPI, WebSocket
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
import asyncio
import time


def build_app(pipeline=None) -> FastAPI:
    app = FastAPI(
        title="ECHOWALL API",
        description="Through-wall sensing API — semantic presence only, no raw CSI.",
        version="0.1.0",
    )

    app.add_middleware(
        CORSMiddleware,
        allow_origins=["*"],
        allow_methods=["GET"],
        allow_headers=["*"],
    )

    @app.get("/health")
    async def health():
        return {"status": "ok", "version": "0.1.0", "timestamp": time.time()}

    @app.get("/presence")
    async def presence():
        """Latest semantic presence result."""
        if pipeline is None:
            return {"presence": False, "count": 0, "posture": "unknown", "confidence": 0.0}
        result = pipeline.get_result()
        if result is None:
            return {"presence": False, "count": 0, "posture": "unknown", "confidence": 0.0}
        return {
            "presence": result.presence,
            "count": result.count,
            "posture": result.posture,
            "breathing_rate": result.breathing_rate,
            "heart_rate": result.heart_rate,
            "confidence": round(result.confidence, 3),
            "timestamp": result.timestamp,
        }

    @app.websocket("/ws")
    async def ws_stream(websocket: WebSocket):
        """Real-time presence stream over WebSocket."""
        await websocket.accept()
        try:
            while True:
                if pipeline:
                    result = pipeline.get_result()
                    if result:
                        await websocket.send_json({
                            "presence": result.presence,
                            "count": result.count,
                            "posture": result.posture,
                            "confidence": round(result.confidence, 3),
                        })
                await asyncio.sleep(0.5)
        except Exception:
            await websocket.close()

    return app
