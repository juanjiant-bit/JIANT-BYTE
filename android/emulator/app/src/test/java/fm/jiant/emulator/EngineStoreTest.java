package fm.jiant.emulator;
import org.junit.Test;
import org.json.*;
import java.nio.file.*;
import java.io.*;
import static org.junit.Assert.*;
public class EngineStoreTest {
 private byte[] wasm(int n){return new byte[]{0,97,115,109,1,0,0,0,0,1,(byte)n};}
 private JSONObject entry(byte[] b)throws Exception{return new JSONObject().put("sha256",EngineStore.sha(b)).put("size",b.length).put("commit","a".repeat(40)).put("version","test");}
 @Test public void activationRollbackAndOfflineRestart()throws Exception{
  File dir=Files.createTempDirectory("engines").toFile();EngineStore s=new EngineStore(dir);byte[] a=wasm(0),b=wasm(1);s.seed("fm-public",entry(a),a);s.flash("fm-public","{\"0\":\"original\"}");
  assertTrue(s.stage("fm-public",entry(b),b));assertEquals(entry(a).getString("sha256"),s.info("fm-public").getJSONObject("active").getString("sha256"));
  assertEquals(entry(b).getString("sha256"),s.begin("fm-public").getString("sha256"));s.flash("fm-public","{\"0\":\"changed\"}");
  s=new EngineStore(dir);assertEquals(entry(a).getString("sha256"),s.begin("fm-public").getString("sha256"));assertEquals("original",new JSONObject(s.flash("fm-public")).getString("0"));assertFalse(s.stage("fm-public",entry(b),b));
 }
 @Test public void validUpdatePersistsAndProfilesAreSeparate()throws Exception{
  File dir=Files.createTempDirectory("engines").toFile();EngineStore s=new EngineStore(dir);byte[] a=wasm(0),b=wasm(2);for(String p:EngineStore.PROFILES)s.seed(p,entry(a),a);s.flash("fm-public","{\"x\":1}");assertEquals("{}",s.flash("byte-public"));s.stage("fm-public",entry(b),b);s.begin("fm-public");s.ready("fm-public");s=new EngineStore(dir);assertEquals(entry(b).getString("sha256"),s.begin("fm-public").getString("sha256"));assertEquals(entry(a).getString("sha256"),s.begin("fm-beta").getString("sha256"));assertTrue(s.rollback("fm-public"));
 }
 @Test public void corruptDownloadCannotReplaceActive()throws Exception{
  EngineStore s=new EngineStore(Files.createTempDirectory("engines").toFile());byte[] a=wasm(0),b=wasm(1);s.seed("byte-public",entry(a),a);
  try{s.stage("byte-public",entry(b),a);fail();}catch(IOException expected){}assertFalse(s.info("byte-public").has("pending"));
  byte[] invalid=new byte[11];try{s.install(entry(invalid),invalid);fail();}catch(IOException expected){}
 }
 @Test public void untrustedSourcesRejected(){assertTrue(UpdateClient.trusted(UpdateClient.CATALOG));assertFalse(UpdateClient.trusted("http://juanjiant-bit.github.io/JIANT/a"));assertFalse(UpdateClient.trusted("https://evil.test/JIANT/a"));assertFalse(UpdateClient.trusted("https://juanjiant-bit.github.io/other/a"));assertFalse(UpdateClient.trusted("https://user@juanjiant-bit.github.io/JIANT/a"));}
 @Test public void errorsAndPendingAreNotReportedAsUpToDate(){
  assertFalse(UpdateClient.summary(0,2,0,"BYTE: tamaño incorrecto").contains("al día"));
  assertTrue(UpdateClient.summary(0,4,2,"").contains("pendientes de abrir"));
  assertTrue(UpdateClient.summary(0,4,0,"").contains("al día"));
 }
 @Test public void canonicalSizeAndDigestRemainMandatory()throws Exception{
  EngineStore s=new EngineStore(Files.createTempDirectory("integrity").toFile());byte[] b=wasm(1);JSONObject e=entry(b);
  try{s.install(new JSONObject(e.toString()).put("size",8),b);fail();}catch(IOException ex){assertTrue(ex.getMessage().contains("tamaño esperado 8"));}
  byte[] changed=b.clone();changed[10]=2;try{s.install(e,changed);fail();}catch(IOException ex){assertTrue(ex.getMessage().contains("SHA-256"));}
  s.install(e,b);assertEquals(e.getString("sha256"),EngineStore.sha(Files.readAllBytes(s.wasm(e).toPath())));
 }
}
