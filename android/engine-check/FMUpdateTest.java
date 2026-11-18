package fm.jiant.emulator;
import android.content.*;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import java.io.*;
import org.json.*;
import org.junit.Test;
import org.junit.runner.RunWith;
import static org.junit.Assert.*;

/** Public APK0.2: bundled FM46/47 to published FM48/49 over real HTTPS, then offline. */
@RunWith(AndroidJUnit4.class)
public class FMUpdateTest extends OfflineTest {
 private File root(){return InstrumentationRegistry.getInstrumentation().getTargetContext().getFilesDir();}
 private EngineStore store()throws Exception{return new EngineStore(new File(root(),"engines"));}
 private void key(MainActivity a,String code,boolean down)throws Exception{js(a,"document.dispatchEvent(new KeyboardEvent('key"+(down?"down":"up")+"',{code:'"+code+"',bubbles:true}));");Thread.sleep(150);}
 private MainActivity boot(String p)throws Exception{
  var i=InstrumentationRegistry.getInstrumentation();var c=i.getTargetContext();MainActivity a=(MainActivity)i.startActivitySync(new Intent(c,MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));i.runOnMainSync(()->a.openProfile(p));
  long end=System.currentTimeMillis()+25000;while(!p.equals(a.bootedProfile)&&System.currentTimeMillis()<end)Thread.sleep(100);assertEquals(a.bootError,p,a.bootedProfile);
  end=System.currentTimeMillis()+20000;while(!"true".equals(js(a,"window.jiantDiagnostics.clock>=3000"))&&System.currentTimeMillis()<end)Thread.sleep(100);assertEquals("true",js(a,"window.jiantDiagnostics.clock>=3000"));return a;
 }
 private void close(MainActivity a)throws Exception{var i=InstrumentationRegistry.getInstrumentation();i.runOnMainSync(a::onBackPressed);Thread.sleep(1600);i.runOnMainSync(a::finish);Thread.sleep(300);}
 @Test public void seedInstalledProfilesOffline()throws Exception{
  assertEquals(2,InstrumentationRegistry.getInstrumentation().getTargetContext().getPackageManager().getPackageInfo("fm.jiant.emulator",0).getLongVersionCode());
  for(String p:new String[]{"fm-public","fm-beta"}){
   MainActivity a=boot(p);String v=store().info(p).getJSONObject("active").getString("version");assertTrue(v,v.contains(p.equals("fm-public")?"46":"47"));
   key(a,"Digit7",true);key(a,"BracketRight",true);key(a,"BracketRight",false);key(a,"Digit7",false);Thread.sleep(1600);close(a);
   String flash=store().flash(p);assertTrue(flash.length()>100);EngineStore.atomic(new File(root(),p+".sha"),EngineStore.sha(flash.getBytes("UTF-8")).getBytes("UTF-8"));
  }
 }
 @Test public void publishedClientDownloadsBothFmUpdates()throws Exception{
  EngineStore s=store();String result=new UpdateClient(s).check();android.util.Log.i("JiantFMUpdate",result);
  assertFalse(result,result.contains("rechazado"));assertFalse(result,result.contains("FM:"));assertFalse(result,result.contains("fm-beta:"));assertFalse(result,result.contains("aún no está publicado"));
  String[] commits={"a6d52c302d816064053f40fb642ff97eb505390e","24d76c013173447935d4572db4628b8d02b25fbe"};int n=0;
  for(String p:new String[]{"fm-public","fm-beta"}){
   JSONObject info=s.info(p);assertEquals(commits[n++],info.getJSONObject("pending").getString("commit"));assertTrue(info.getJSONObject("active").getString("version").contains(p.equals("fm-public")?"46":"47"));
   assertEquals(EngineStore.readText(new File(root(),p+".sha")),EngineStore.sha(s.flash(p).getBytes("UTF-8")));
  }
 }
 @Test public void updatedFmEnginesPlayOffline()throws Exception{
  for(String p:new String[]{"fm-public","fm-beta"}){
   MainActivity a=boot(p);JSONObject info=store().info(p);assertFalse(info.has("pending"));assertFalse(info.optBoolean("trial"));assertTrue(info.getJSONObject("active").getString("version").contains(p.equals("fm-public")?"48":"49"));
   key(a,"KeyZ",true);key(a,"KeyX",true);Thread.sleep(700);assertEquals("true",js(a,"window.jiantDiagnostics.peak>0.00001"));key(a,"KeyZ",false);key(a,"KeyX",false);assertEquals("0",js(a,"window.jiantDiagnostics.keys()"));
   screenshot(a,p+"-updated-offline");close(a);
   assertEquals(EngineStore.readText(new File(root(),p+".sha")),EngineStore.sha(java.nio.file.Files.readAllBytes(new File(root(),"engines/state/"+p+".previous").toPath())));
  }
 }
}
