/* SPDX-License-Identifier: GPL-3.0-only */
package fm.jiant.emulator;

import android.app.*;
import android.os.*;
import android.graphics.Color;
import android.view.*;
import android.webkit.*;
import android.widget.*;
import android.media.*;
import java.io.*;
import java.util.*;
import java.util.concurrent.*;
import org.json.*;

public class MainActivity extends Activity {
 private EngineStore store;
 private final ExecutorService worker=Executors.newSingleThreadExecutor();
 private final Handler handler=new Handler(Looper.getMainLooper());
 private WebView web;
 private LinearLayout root;
 private TextView notice;
 private String profile,updateStatus="Los motores incluidos funcionan sin conexión.";
 private boolean checking=false,closing=false,foreground=false;
 private long lastCheck=0;
 private Bridge bridge;
 private AudioManager audio;
 private AudioFocusRequest focus;
 public volatile String bootedProfile="";
 public volatile String bootError="";
 private int dp(int n){return (int)(n*getResources().getDisplayMetrics().density);}
 private byte[] asset(String name)throws Exception {try(InputStream in=getAssets().open(name);ByteArrayOutputStream out=new ByteArrayOutputStream()){byte[] b=new byte[16384];int n;while((n=in.read(b))!=-1)out.write(b,0,n);return out.toByteArray();}}
 @Override public void onCreate(Bundle state){
  super.onCreate(state);getWindow().setStatusBarColor(Color.rgb(19,12,24));getWindow().setNavigationBarColor(Color.rgb(19,12,24));
  audio=(AudioManager)getSystemService(AUDIO_SERVICE);
  focus=new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN).setAudioAttributes(new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_MEDIA).setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build()).setOnAudioFocusChangeListener(change->{if(change<0)closePlayer();},handler).build();
  try{store=new EngineStore(new File(getFilesDir(),"engines"));JSONObject entries=new JSONObject(new String(asset("engines.json"),"UTF-8")).getJSONObject("profiles");for(String p:EngineStore.PROFILES){JSONObject e=entries.getJSONObject(p);store.seed(p,e,asset("engines/"+e.getString("sha256")+".wasm"));}selector();}catch(Exception e){TextView error=new TextView(this);error.setText("No se pudo abrir el almacén local: "+e.getMessage());setContentView(error);}
 }
 @Override public void setContentView(View content){
  super.setContentView(content);int left=content.getPaddingLeft(),top=content.getPaddingTop(),right=content.getPaddingRight(),bottom=content.getPaddingBottom();
  content.setOnApplyWindowInsetsListener((v,insets)->{v.setPadding(left+insets.getSystemWindowInsetLeft(),top+insets.getSystemWindowInsetTop(),right+insets.getSystemWindowInsetRight(),bottom+insets.getSystemWindowInsetBottom());return insets.consumeSystemWindowInsets();});content.requestApplyInsets();
 }
 private TextView text(String s,int size){TextView t=new TextView(this);t.setText(s);t.setTextSize(size);t.setTextColor(Color.rgb(255,231,205));t.setPadding(dp(12),dp(8),dp(12),dp(8));return t;}
 private Button button(String label,Runnable action){Button b=new Button(this);b.setText(label);b.setAllCaps(false);b.setOnClickListener(v->action.run());return b;}
 private void selector(){
  profile=null;bootedProfile="";root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setPadding(dp(12),dp(10),dp(12),dp(10));root.setBackgroundColor(Color.rgb(19,12,24));ScrollView scroll=new ScrollView(this);scroll.addView(root);setContentView(scroll);
  root.addView(text("JIANT / OFFLINE",28));root.addView(text("FM + /B1T3 · Android 0.1 beta",16));
  for(String p:EngineStore.PROFILES)try{JSONObject info=store.info(p),active=info.getJSONObject("active");String label=(p.startsWith("fm")?"JIANT FM":"JIANT /B1T3")+" · "+(p.endsWith("beta")?"BETA":"PUBLICADO");root.addView(button(label,()->openProfile(p)));root.addView(text(active.getString("version")+" · "+active.getString("commit").substring(0,7)+(info.has("pending")?"\nNueva versión descargada: se aplica al abrir.":""),13));if(info.has("previous"))root.addView(button("Volver a la versión anterior",()->{try{store.rollback(p);selector();}catch(Exception e){showError(e);}}));}catch(Exception e){showError(e);}
  root.addView(button("Buscar actualizaciones",this::checkUpdates));notice=text(updateStatus,14);root.addView(notice);
  root.addView(text("Proyectos separados por motor y canal. SAVE guarda en este teléfono. Al salir se detiene el audio. Las actualizaciones del motor se aplican al abrirlo; la app se actualiza instalando un APK nuevo.\n\nMotor WebAssembly en Android WebView, sin conexión. Actualizá Android System WebView si el audio no inicia.\n\nGPL-3.0 · Juan Jiant · Hügelton Instruments / Leo Kuroshita · Charles Vestal. DotGothic16: SIL OFL. Código: github.com/juanjiant-bit/JIANT-BYTE",12));
 }
 private void showError(Exception e){Toast.makeText(this,e.getMessage(),Toast.LENGTH_LONG).show();}
 public void openProfile(String p){
  if(web!=null||store==null)return;
  try{
   JSONObject entry=store.begin(p);File engine=store.wasm(entry);profile=p;closing=false;bootError="";bootedProfile="";
   root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setBackgroundColor(Color.BLACK);
   root.addView(button("‹ Motores    "+entry.getString("version"),this::closePlayer));
   WebView current=new WebView(this);web=current;bridge=new Bridge(p,current);Bridge currentBridge=bridge;
   WebSettings settings=current.getSettings();settings.setJavaScriptEnabled(true);settings.setDomStorageEnabled(false);settings.setAllowFileAccess(false);settings.setAllowContentAccess(false);settings.setBlockNetworkLoads(true);settings.setMediaPlaybackRequiresUserGesture(false);settings.setSupportZoom(false);settings.setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);
   current.addJavascriptInterface(currentBridge,"AndroidEngine");
   String host=p+".jiant.local";
   current.setWebViewClient(new WebViewClient(){
    @Override public boolean shouldOverrideUrlLoading(WebView v,WebResourceRequest r){return true;}
    @Override public WebResourceResponse shouldInterceptRequest(WebView v,WebResourceRequest request){
     try{android.net.Uri uri=request.getUrl();if(!"https".equals(uri.getScheme())||!host.equals(uri.getHost()))return denied();String path=uri.getPath();InputStream in;String mime;
      if("/felucca.wasm".equals(path)){in=new FileInputStream(engine);mime="application/wasm";}
      else {Map<String,String> allowed=new HashMap<>();allowed.put("/index.html","text/html");allowed.put("/worklet.js","text/javascript");allowed.put("/fonts/DotGothic16-subset.woff","font/woff");mime=allowed.get(path);if(mime==null)return denied();in=getAssets().open("shell"+path);}
      Map<String,String> headers=new HashMap<>();headers.put("Cache-Control","no-store");headers.put("Content-Security-Policy","default-src 'self'; script-src 'self' 'unsafe-inline' 'unsafe-eval' 'wasm-unsafe-eval'; style-src 'self' 'unsafe-inline'; connect-src 'self'; object-src 'none'; base-uri 'none'");
      return new WebResourceResponse(mime,"UTF-8",200,"OK",headers,in);
     }catch(Exception e){return denied();}
    }
    @Override public void onPageFinished(WebView v,String url){if(web==v&&foreground)v.evaluateJavascript("window.jiantStart && window.jiantStart()",null);}
   });
   root.addView(current,new LinearLayout.LayoutParams(-1,0,1));setContentView(root);getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
   audio.requestAudioFocus(focus);current.loadUrl("https://"+host+"/index.html");
   handler.postDelayed(()->{if(web==current&&!currentBridge.ready&&!closing)currentBridge.failed("El motor no inició. Revisá Android System WebView.");},25000);
  }catch(Exception e){try{store.rollback(p);}catch(Exception ignored){}selector();showError(e);}
 }
 private WebResourceResponse denied(){return new WebResourceResponse("text/plain","UTF-8",403,"Blocked",Collections.emptyMap(),new ByteArrayInputStream(new byte[0]));}
 public final class Bridge {
  final String p;final WebView view;volatile boolean valid=true,ready=false;
  Bridge(String p,WebView v){this.p=p;view=v;}
  @JavascriptInterface public String flash(){try{return valid?store.flash(p):"{}";}catch(Exception e){return "{}";}}
  @JavascriptInterface public boolean save(String data){if(!valid)return false;try{store.flash(p,data);return true;}catch(Exception e){return false;}}
  @JavascriptInterface public void ready(){if(!valid)return;try{store.ready(p);ready=true;bootedProfile=p;}catch(Exception e){failed(e.getMessage());}}
  @JavascriptInterface public void failed(String message){handler.post(()->{if(!valid||web!=view)return;bootError=message;try{if(store.info(p).optBoolean("trial"))store.rollback(p);}catch(Exception ignored){}Toast.makeText(MainActivity.this,message,Toast.LENGTH_LONG).show();finishPlayer(view);});}
  @JavascriptInterface public void paused(){handler.post(()->finishPlayer(view));}
 }
 private void closePlayer(){if(web==null||closing)return;closing=true;WebView current=web;current.evaluateJavascript("window.jiantPause ? window.jiantPause() : AndroidEngine.paused()",null);handler.postDelayed(()->finishPlayer(current),2500);}
 private void finishPlayer(WebView current){if(web!=current)return;if(bridge!=null)bridge.valid=false;current.removeJavascriptInterface("AndroidEngine");current.stopLoading();root.removeView(current);current.destroy();web=null;bridge=null;closing=false;audio.abandonAudioFocusRequest(focus);getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);selector();}
 private void checkUpdates(){if(checking||store==null)return;checking=true;lastCheck=System.currentTimeMillis();updateStatus="Buscando versiones… El audio no se interrumpe.";if(notice!=null)notice.setText(updateStatus);worker.execute(()->{String result=new UpdateClient(store).check();handler.post(()->{checking=false;updateStatus=result;if(web==null&&!isFinishing())selector();});});}
 private final Runnable periodic=new Runnable(){public void run(){if(foreground){if(System.currentTimeMillis()-lastCheck>15*60*1000)checkUpdates();handler.postDelayed(this,60000);}}};
 @Override protected void onResume(){super.onResume();foreground=true;handler.removeCallbacks(periodic);handler.post(periodic);}
 @Override protected void onPause(){foreground=false;handler.removeCallbacks(periodic);closePlayer();super.onPause();}
 @Override public void onBackPressed(){if(web!=null)closePlayer();else super.onBackPressed();}
 @Override protected void onDestroy(){if(web!=null)finishPlayer(web);handler.removeCallbacksAndMessages(null);worker.shutdownNow();super.onDestroy();}
}
