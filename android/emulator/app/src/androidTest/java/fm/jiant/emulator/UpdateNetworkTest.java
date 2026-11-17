package fm.jiant.emulator;
import android.content.*;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import java.io.*;
import org.json.*;
import org.junit.Test;
import org.junit.runner.RunWith;
import static org.junit.Assert.*;

/** Runs against the actual 0.1 APK first, then install -r 0.2, then offline. */
@RunWith(AndroidJUnit4.class)
public class UpdateNetworkTest extends OfflineTest {
 private File root(){return InstrumentationRegistry.getInstrumentation().getTargetContext().getFilesDir();}
 private EngineStore store()throws Exception{return new EngineStore(new File(root(),"engines"));}
 private void key(MainActivity a,String code,boolean down)throws Exception{js(a,"document.dispatchEvent(new KeyboardEvent('key"+(down?"down":"up")+"',{code:'"+code+"',bubbles:true}));");Thread.sleep(150);}
 private MainActivity boot(String p)throws Exception{
  var i=InstrumentationRegistry.getInstrumentation();var c=i.getTargetContext();MainActivity a=(MainActivity)i.startActivitySync(new Intent(c,MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));i.runOnMainSync(()->a.openProfile(p));
  long end=System.currentTimeMillis()+25000;while(!p.equals(a.bootedProfile)&&System.currentTimeMillis()<end)Thread.sleep(100);assertEquals(a.bootError,p,a.bootedProfile);
  end=System.currentTimeMillis()+20000;while(!"true".equals(js(a,"window.jiantDiagnostics.clock>=3000"))&&System.currentTimeMillis()<end)Thread.sleep(100);assertEquals("true",js(a,"window.jiantDiagnostics.clock>=3000"));return a;
 }
 private void close(MainActivity a)throws Exception{var i=InstrumentationRegistry.getInstrumentation();i.runOnMainSync(a::onBackPressed);Thread.sleep(1600);i.runOnMainSync(a::finish);Thread.sleep(300);}
 @Test public void seedLegacyOffline()throws Exception{
  assertEquals(1,InstrumentationRegistry.getInstrumentation().getTargetContext().getPackageManager().getPackageInfo("fm.jiant.emulator",0).getLongVersionCode());
  MainActivity a=boot("byte-public");assertTrue(store().info("byte-public").getJSONObject("active").getString("version").contains("0.5"));
  // A real firmware setting saved through HOME + OCT+; flash travels through AudioWorklet/Java bridge.
  key(a,"Digit7",true);key(a,"BracketRight",true);key(a,"BracketRight",false);key(a,"Digit7",false);Thread.sleep(1800);close(a);
  String flash=store().flash("byte-public");assertTrue("Firmware saved sectors: "+flash.length(),flash.length()>100);
  EngineStore.atomic(new File(root(),"upgrade-flash.sha"),EngineStore.sha(flash.getBytes("UTF-8")).getBytes("UTF-8"));
 }
 @Test public void reproduceLegacyNetworkFailure()throws Exception{
  EngineStore s=store();String result=new UpdateClient(s).check();android.util.Log.i("JiantUpgrade","LEGACY: "+result);
  assertTrue("Old APK must reproduce user rejection: "+result,result.contains("BYTE: Motor rechazado"));
  assertFalse(s.info("byte-public").has("pending"));assertTrue(s.info("byte-public").getJSONObject("active").getString("version").contains("0.5"));
 }
 @Test public void updateOverNetworkAfterApkUpgrade()throws Exception{
  assertEquals(2,InstrumentationRegistry.getInstrumentation().getTargetContext().getPackageManager().getPackageInfo("fm.jiant.emulator",0).getLongVersionCode());
  EngineStore s=store();assertEquals(EngineStore.readText(new File(root(),"upgrade-flash.sha")),EngineStore.sha(s.flash("byte-public").getBytes("UTF-8")));
  assertTrue(s.info("byte-public").getJSONObject("active").getString("version").contains("0.5"));
  String result=new UpdateClient(s).check();android.util.Log.i("JiantUpgrade","UPDATED: "+result);assertFalse(result,result.contains("rechazado"));assertFalse(result,result.contains("BYTE:"));assertFalse(result,result.contains("byte-beta:"));
  JSONObject live=UpdateClient.json(UpdateClient.CATALOG).getJSONObject("profiles");
  for(String p:new String[]{"byte-public","byte-beta"}){JSONObject info=s.info(p);assertEquals(live.getJSONObject(p).getString("sha256"),info.getJSONObject("pending").getString("sha256"));assertTrue(info.getJSONObject("active").getString("version").contains("0.5"));}
  assertEquals(EngineStore.readText(new File(root(),"upgrade-flash.sha")),EngineStore.sha(s.flash("byte-public").getBytes("UTF-8")));
  assertTrue(new UpdateClient(s).check().contains("pendientes de abrir"));
 }
 @Test public void updatedEngineOpensOfflineWithSavedFlash()throws Exception{
  EngineStore s=store();assertEquals(EngineStore.readText(new File(root(),"upgrade-flash.sha")),EngineStore.sha(s.flash("byte-public").getBytes("UTF-8")));
  for(String p:new String[]{"byte-public","byte-beta"}){
   MainActivity a=boot(p);JSONObject info=store().info(p);assertFalse(info.has("pending"));assertFalse(info.optBoolean("trial"));assertTrue(info.getJSONObject("active").getString("version").contains("0.7"));
   key(a,"KeyZ",true);Thread.sleep(700);assertEquals("true",js(a,"window.jiantDiagnostics.peak>0.00001"));key(a,"KeyZ",false);screenshot(a,p+"-updated-offline");close(a);
  }
  // Activation snapshots the exact user flash before the new firmware can migrate it.
  assertEquals(EngineStore.readText(new File(root(),"upgrade-flash.sha")),EngineStore.sha(java.nio.file.Files.readAllBytes(new File(root(),"engines/state/byte-public.previous").toPath())));
 }
}
