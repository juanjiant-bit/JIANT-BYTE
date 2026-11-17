/* SPDX-License-Identifier: GPL-3.0-only */
package fm.jiant.emulator;

import org.json.*;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;

/** Content-addressed WASM and transactional profile pointers. No executable JS downloads. */
public final class EngineStore {
 public static final int MAX_ENGINE=12*1024*1024, MAX_STATE=2*1024*1024;
 public static final String[] PROFILES={"fm-public","byte-public","fm-beta","byte-beta"};
 private final File root;
 private JSONObject profiles;
 public EngineStore(File dir) throws Exception {
  root=dir;root.mkdirs();new File(root,"wasm").mkdirs();new File(root,"state").mkdirs();
  File p=new File(root,"profiles.json");profiles=p.exists()?new JSONObject(readText(p)):new JSONObject();
  for(String key:PROFILES) if(profiles.has(key)&&profiles.getJSONObject(key).optBoolean("trial")) rollback(key);
 }
 public static boolean validProfile(String p){return Arrays.asList(PROFILES).contains(p);}
 public static String sha(byte[] data) throws Exception {
  byte[] h=MessageDigest.getInstance("SHA-256").digest(data);StringBuilder s=new StringBuilder();for(byte b:h)s.append(String.format(Locale.ROOT,"%02x",b&255));return s.toString();
 }
 public static void validate(JSONObject e) throws Exception {
  if(!e.getString("sha256").matches("[0-9a-f]{64}")||!e.getString("commit").matches("[0-9a-f]{40}")||e.getString("version").length()>100||e.getInt("size")<8||e.getInt("size")>MAX_ENGINE)throw new IOException("Metadatos de motor inválidos");
 }
 public synchronized File wasm(JSONObject e){return new File(root,"wasm/"+e.optString("sha256")+".wasm");}
 public synchronized void install(JSONObject e,byte[] data) throws Exception {
  validate(e);
  if(data.length!=e.getInt("size"))throw new IOException("Motor rechazado: tamaño esperado "+e.getInt("size")+", recibido "+data.length+" bytes");
  if(data[0]!=0||data[1]!=97||data[2]!=115||data[3]!=109||data[4]!=1||data[5]!=0||data[6]!=0||data[7]!=0)throw new IOException("Motor rechazado: formato WASM inválido");
  if(!sha(data).equals(e.getString("sha256")))throw new IOException("Motor rechazado: SHA-256 incorrecto");
  atomic(wasm(e),data);
 }
 public synchronized void seed(String profile,JSONObject entry,byte[] data) throws Exception {
  if(!validProfile(profile))throw new IOException("Perfil inválido");
  if(!wasm(entry).exists())install(entry,data);
  if(!profiles.has(profile)){profiles.put(profile,new JSONObject().put("active",entry));save();}
 }
 public synchronized JSONObject info(String profile) throws Exception {
  if(!validProfile(profile))throw new IOException("Perfil inválido");return new JSONObject(profiles.getJSONObject(profile).toString());
 }
 public synchronized boolean stage(String profile,JSONObject e,byte[] data) throws Exception {
  JSONObject p=profiles.getJSONObject(profile);validate(e);
  String hash=e.getString("sha256");
  if(hash.equals(p.getJSONObject("active").getString("sha256"))||hash.equals(p.optString("rejected")))return false;
  if(p.has("pending")&&hash.equals(p.getJSONObject("pending").getString("sha256")))return false;
  install(e,data);p.put("pending",e);save();return true;
 }
 public synchronized JSONObject begin(String profile) throws Exception {
  JSONObject p=profiles.getJSONObject(profile);
  if(p.has("pending")){
   File flash=stateFile(profile);atomic(new File(root,"state/"+profile+".previous"),flash.exists()?Files.readAllBytes(flash.toPath()):"{}".getBytes("UTF-8"));
   p.put("previous",p.getJSONObject("active"));p.put("active",p.getJSONObject("pending"));p.remove("pending");p.put("trial",true);save();
  }
  JSONObject e=p.getJSONObject("active");
  byte[] data=Files.readAllBytes(wasm(e).toPath());
  if(data.length!=e.getInt("size")||!sha(data).equals(e.getString("sha256")))throw new IOException("Motor local corrupto");
  return new JSONObject(e.toString());
 }
 public synchronized void ready(String profile) throws Exception {profiles.getJSONObject(profile).put("trial",false);save();}
 public synchronized boolean rollback(String profile) throws Exception {
  JSONObject p=profiles.getJSONObject(profile);if(!p.has("previous")){p.put("trial",false);save();return false;}
  p.put("rejected",p.getJSONObject("active").getString("sha256"));p.put("active",p.getJSONObject("previous"));p.remove("previous");p.remove("pending");p.put("trial",false);
  File backup=new File(root,"state/"+profile+".previous");if(backup.exists())atomic(stateFile(profile),Files.readAllBytes(backup.toPath()));save();return true;
 }
 private File stateFile(String profile) throws IOException {if(!validProfile(profile))throw new IOException("Perfil inválido");return new File(root,"state/"+profile+".json");}
 public synchronized String flash(String profile) throws Exception {File f=stateFile(profile);return f.exists()?readText(f):"{}";}
 public synchronized void flash(String profile,String value) throws Exception {
  if(value.length()>MAX_STATE)throw new IOException("Estado demasiado grande");new JSONObject(value);atomic(stateFile(profile),value.getBytes("UTF-8"));
 }
 private void save() throws Exception {atomic(new File(root,"profiles.json"),profiles.toString().getBytes("UTF-8"));}
 public static String readText(File f) throws IOException {return new String(Files.readAllBytes(f.toPath()),"UTF-8");}
 public static void atomic(File f,byte[] b) throws IOException {
  File tmp=new File(f.getPath()+".tmp");try(FileOutputStream o=new FileOutputStream(tmp)){o.write(b);o.getFD().sync();}
  Files.move(tmp.toPath(),f.toPath(),StandardCopyOption.ATOMIC_MOVE,StandardCopyOption.REPLACE_EXISTING);
 }
}
