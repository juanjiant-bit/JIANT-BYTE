/* SPDX-License-Identifier: GPL-3.0-only */
package fm.jiant.emulator;
import org.json.*;
import java.io.*;
import java.net.*;
import javax.net.ssl.HttpsURLConnection;

/** Only publisher HTTPS endpoints; digest comes from an independently fetched manifest. */
public final class UpdateClient {
 public static final String CATALOG="https://juanjiant-bit.github.io/JIANT-BYTE/android/engines.json";
 private static final String BYTE="https://juanjiant-bit.github.io/JIANT-BYTE/",FM="https://juanjiant-bit.github.io/JIANT/";
 private final EngineStore store;
 public UpdateClient(EngineStore s){store=s;}
 static boolean trusted(String url){
  try{URI u=new URI(url);String host=u.getHost(),path=u.getPath();return "https".equals(u.getScheme())&&u.getUserInfo()==null&&u.getPort()==-1&&(("juanjiant-bit.github.io".equals(host)&&(path.startsWith("/JIANT/")||path.startsWith("/JIANT-BYTE/")))||("raw.githubusercontent.com".equals(host)&&path.startsWith("/juanjiant-bit/JIANT/main/web/")));}catch(Exception e){return false;}
 }
 static byte[] fetch(String url,int max) throws Exception {
  if(!trusted(url))throw new IOException("Origen de actualización no permitido");
  HttpsURLConnection c=(HttpsURLConnection)new URL(url).openConnection();c.setInstanceFollowRedirects(false);c.setConnectTimeout(10000);c.setReadTimeout(15000);c.setRequestProperty("Cache-Control","no-cache");
  try{
   if(c.getResponseCode()!=200)throw new IOException("HTTP "+c.getResponseCode());
   if(c.getContentLengthLong()>max)throw new IOException("Descarga demasiado grande");
   try(InputStream in=c.getInputStream();ByteArrayOutputStream out=new ByteArrayOutputStream()){
    byte[] b=new byte[16384];int n;while((n=in.read(b))!=-1){if(out.size()+n>max)throw new IOException("Descarga demasiado grande");out.write(b,0,n);}return out.toByteArray();
   }
  }finally{c.disconnect();}
 }
 static JSONObject json(String url) throws Exception{return new JSONObject(new String(fetch(url,512*1024),"UTF-8"));}
 private boolean stage(String profile,JSONObject e) throws Exception {
  EngineStore.validate(e);if(!trusted(e.getString("url")))throw new IOException("Origen de motor no permitido");
  JSONObject p=store.info(profile);String hash=e.getString("sha256");
  if(hash.equals(p.getJSONObject("active").getString("sha256"))||hash.equals(p.optString("rejected"))||p.has("pending")&&hash.equals(p.getJSONObject("pending").getString("sha256")))return false;
  return store.stage(profile,e,fetch(e.getString("url"),EngineStore.MAX_ENGINE));
 }
 private JSONObject bytePublic() throws Exception {
  JSONObject b=json(BYTE+"emu/build.json");JSONArray versions=new JSONArray(new String(fetch(BYTE+"firmware/versions.json",512*1024),"UTF-8"));JSONObject v=versions.getJSONObject(0);
  if(!b.getString("commit").equals(v.optString("commit")))throw new IOException("Publicación BYTE todavía en curso");
  // Manifest does not advertise WASM length. Read the HTTP header, then enforce digest and length during download.
  String url=BYTE+"emu/felucca.wasm";HttpsURLConnection c=(HttpsURLConnection)new URL(url).openConnection();c.setRequestMethod("HEAD");c.setConnectTimeout(10000);c.setReadTimeout(10000);long size;
  try{if(c.getResponseCode()!=200)throw new IOException("WASM no disponible");size=c.getContentLengthLong();}finally{c.disconnect();}
  return new JSONObject().put("version",v.getString("version")).put("commit",b.getString("commit")).put("sha256",b.getString("wasm_sha256")).put("size",size).put("url",url).put("source_repo","juanjiant-bit/JIANT-BYTE");
 }
 private JSONObject fmPublic() throws Exception {
  JSONObject release=json("https://raw.githubusercontent.com/juanjiant-bit/JIANT/main/web/EMULATOR_BETA.json");
  if(!"public".equals(release.optString("channel")))throw new IOException("Emulador FM oficial está en beta");
  String source=new String(fetch(FM+"emu/SOURCE_COMMIT",256),"UTF-8").trim();if(!source.equals(release.getString("source")))throw new IOException("Publicación FM todavía en curso");
  JSONObject file=json(FM+"site-manifest.json").getJSONObject("files").getJSONObject("emu/felucca.wasm");
  return new JSONObject().put("version",release.getString("version")).put("commit",source).put("sha256",file.getString("sha256")).put("size",file.getLong("size")).put("url",FM+"emu/felucca.wasm").put("source_repo","juanjiant-bit/JIANT");
 }
 public String check() {
  int installed=0,ok=0;StringBuilder errors=new StringBuilder();JSONObject catalog=null,byteEntry=null;
  try{catalog=json(CATALOG);if(catalog.getInt("schema")!=1||catalog.optInt("min_app",1)>1)throw new IOException("Necesita actualizar la app");}catch(Exception e){catalog=null;errors.append("Catálogo: ").append(e.getMessage()).append(". ");}
  try{byteEntry=bytePublic();if(stage("byte-public",byteEntry))installed++;ok++;}catch(Exception e){errors.append("BYTE: ").append(e.getMessage()).append(". ");}
  try{if(stage("fm-public",fmPublic()))installed++;ok++;}catch(Exception e){errors.append("FM: ").append(e.getMessage()).append(". ");}
  if(catalog!=null)for(String profile:new String[]{"fm-beta","byte-beta"})try{
   JSONObject e=catalog.getJSONObject("profiles").getJSONObject(profile);
   if(profile.equals("byte-beta")&&byteEntry!=null&&e.getString("commit").equals(catalog.getJSONObject("profiles").getJSONObject("byte-public").getString("commit")))e=byteEntry;
   if(stage(profile,e))installed++;ok++;
  }catch(Exception e){errors.append(profile).append(": ").append(e.getMessage()).append(". ");}
  if(catalog!=null)try{
   JSONObject latest=json("https://raw.githubusercontent.com/juanjiant-bit/JIANT/main/web/SPLIT_BETA.json");
   if(!latest.getString("source").equals(catalog.getJSONObject("profiles").getJSONObject("fm-beta").getString("commit")))errors.append("FM tiene una beta de firmware más nueva; su emulador compatible aún no está publicado. ");
  }catch(Exception ignored){}
  return (installed>0?installed+" motor(es) descargado(s). Se aplican al abrir el perfil, sin cortar el audio. ":ok>0?"Motores disponibles al día. ":"Sin conexión: se conservan los motores instalados. ")+errors;
 }
}
