package com.example.aplicacionnativa;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.Bundle;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.webkit.WebView;
import android.widget.SeekBar;

import com.example.aplicacionnativa.databinding.ActivityMainBinding;

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("aplicacionnativa");
    }

    private ActivityMainBinding binding;
    private WebView webView;
    private SurfaceView surfaceView;
    private SurfaceHolder surfaceHolder;
    private SeekBar seekBar, seekBar2, seekBar3;

    private int glitchIntensity = 40;
    private int noiseAmount = 20;
    private int edgeColorChange = 180;

    private Bitmap bitmapIn, bitmapOut;
    private boolean isProcessing = false;
    private Thread processingThread;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        webView = findViewById(R.id.webView);
        webView.getSettings().setJavaScriptEnabled(true);
        webView.loadUrl("http://192.168.84.35:81/stream");

        seekBar = findViewById(R.id.seekBar);
        seekBar2 = findViewById(R.id.seekBar2);
        seekBar3 = findViewById(R.id.seekBar3);

        seekBar.setMax(100);
        seekBar.setProgress(noiseAmount);
        seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                noiseAmount = progress;
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                processFrame();
            }
        });

        seekBar2.setMax(25);
        seekBar2.setProgress(edgeColorChange);
        seekBar2.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                edgeColorChange = progress;
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                processFrame();
            }
        });

        seekBar3.setMax(100);
        seekBar3.setProgress(glitchIntensity);
        seekBar3.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                glitchIntensity = progress;
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                processFrame();
            }
        });

        surfaceView = findViewById(R.id.surfaceView);
        surfaceHolder = surfaceView.getHolder();

        surfaceView.addOnLayoutChangeListener((v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {});

        surfaceHolder.addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(@NonNull SurfaceHolder holder) {
                Log.d("SurfaceView", "Surface creado, comenzando el procesamiento");

                int width = surfaceView.getWidth();
                int height = surfaceView.getHeight();

                if (bitmapIn == null || bitmapOut == null || bitmapIn.getWidth() != width || bitmapIn.getHeight() != height) {
                    bitmapIn = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
                    bitmapOut = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
                    Log.d("SurfaceView", "BitmapIn y BitmapOut creados con tamaño: " + width + "x" + height);
                }

                startContinuousProcessing();
            }

            @Override
            public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
                if (bitmapIn == null || bitmapOut == null || bitmapIn.getWidth() != width || bitmapIn.getHeight() != height) {
                    bitmapIn = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
                    bitmapOut = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
                    Log.d("SurfaceView", "BitmapIn y BitmapOut creados con tamaño: " + width + "x" + height);
                }
                processFrame();
            }

            @Override
            public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
                if (bitmapIn != null) {
                    bitmapIn.recycle();
                    bitmapIn = null;
                }
                if (bitmapOut != null) {
                    bitmapOut.recycle();
                    bitmapOut = null;
                }
                stopContinuousProcessing();
            }
        });
    }


    private void captureWebViewFrame() {
        if (webView != null) {
            if (bitmapIn != null && bitmapOut != null) {
                try {
                    webView.draw(new Canvas(bitmapIn));
                } catch (Exception e) {
                    Log.e("MainActivity", "Error al capturar el fotograma: ", e);
                }
            } else {
                Log.e("MainActivity", "bitmapIn o bitmapOut no están inicializados correctamente.");
            }
        }
    }


    private void processFrame() {
        if (bitmapIn != null && bitmapIn.getWidth() > 0 && bitmapIn.getHeight() > 0) {
            Log.d("SurfaceView", "Procesando fotograma");
            captureWebViewFrame();
            applyGlitchEffect(bitmapIn, bitmapOut, glitchIntensity, noiseAmount, edgeColorChange);
            drawToSurface();
        } else {
            Log.e("SurfaceView", "Bitmap vacío o no inicializado correctamente");
        }
    }


    private void drawToSurface() {
        Canvas canvas = surfaceHolder.lockCanvas();
        if (canvas != null) {
            try {
                Log.d("SurfaceView", "Dibujando bitmap filtrado en el canvas");
                canvas.drawBitmap(bitmapOut, 0, 0, null);
            } finally {
                surfaceHolder.unlockCanvasAndPost(canvas);
                Log.d("SurfaceView", "Canvas desbloqueado y enviado al SurfaceView");
            }
        } else {
            Log.e("SurfaceView", "No se pudo obtener el canvas");
        }
    }


    private void startContinuousProcessing() {
        isProcessing = true;
        processingThread = new Thread(new Runnable() {
            @Override
            public void run() {
                while (isProcessing) {
                    if (bitmapIn != null && bitmapIn.getWidth() > 0 && bitmapIn.getHeight() > 0) {
                        processFrame();
                        try {
                            Thread.sleep(30);
                        } catch (InterruptedException e) {
                            e.printStackTrace();
                        }
                    }
                }
            }
        });
        processingThread.start();
    }

    private void stopContinuousProcessing() {
        isProcessing = false;
        try {
            if (processingThread != null && processingThread.isAlive()) {
                processingThread.join();
            }
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    public native void applyGlitchEffect(Bitmap in, Bitmap out, int glitchIntensity, int noiseAmount, int edgeColorChange);
}

