package fm.jiant.emulator;
import android.content.Intent;
import android.webkit.WebView;
import android.view.View;
import android.view.ViewGroup;
import java.util.concurrent.*;
import java.io.*;
import android.graphics.Bitmap;
import android.content.pm.ActivityInfo;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import org.junit.Test;
import org.junit.runner.RunWith;
import static org.junit.Assert.*;
@RunWith(AndroidJUnit4.class)
public class OfflineTest {
 private WebView web(View v){if(v instanceof WebView)return (WebView)v;if(v instanceof ViewGroup)for(int i=0;i<((ViewGroup)v).getChildCount();i++){WebView found=web(((ViewGroup)v).getChildAt(i));if(found!=null)return found;}return null;}
 private String js(MainActivity a,String code)throws Exception {CountDownLatch done=new CountDownLatch(1);String[] result={null};InstrumentationRegistry.getInstrumentation().runOnMainSync(()->web(a.getWindow().getDecorView()).evaluateJavascript(code,r->{result[0]=r;done.countDown();}));assertTrue(done.await(5,TimeUnit.SECONDS));return result[0];}
 private void screenshot(MainActivity a,String name)throws Exception {File dir=new File(a.getExternalFilesDir(null),"screenshots");dir.mkdirs();try(FileOutputStream out=new FileOutputStream(new File(dir,name+".png"))){InstrumentationRegistry.getInstrumentation().getUiAutomation().takeScreenshot().compress(Bitmap.CompressFormat.PNG,100,out);}}

 @Test public void allBundledProfilesBootWithoutNetworkAndRotate()throws Exception{
  var instrumentation=InstrumentationRegistry.getInstrumentation();var context=instrumentation.getTargetContext();
  MainActivity activity=(MainActivity)instrumentation.startActivitySync(new Intent(context,MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
  for(String p:EngineStore.PROFILES){
   instrumentation.runOnMainSync(()->activity.openProfile(p));
   long deadline=System.currentTimeMillis()+25000;
   while(!p.equals(activity.bootedProfile)&&activity.bootError.isEmpty()&&System.currentTimeMillis()<deadline)Thread.sleep(100);
   assertEquals("Engine error: "+activity.bootError,p,activity.bootedProfile);
   deadline=System.currentTimeMillis()+20000;
   while(!"true".equals(js(activity,"window.jiantDiagnostics.clock>=3000"))&&System.currentTimeMillis()<deadline)Thread.sleep(200);
   assertEquals(p+" firmware clock: "+js(activity,"JSON.stringify(window.jiantDiagnostics)"),"true",js(activity,"window.jiantDiagnostics.clock>=3000"));
   js(activity,"document.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyZ',key:'z',bubbles:true}));document.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyX',key:'x',bubbles:true}));");Thread.sleep(1000);
   assertEquals(p+" held keys","true",js(activity,"window.jiantDiagnostics.keys()!==0"));
   assertEquals(p+" audio: "+js(activity,"JSON.stringify({...window.jiantDiagnostics,audioState:window.jiantDiagnostics.audioState(),keys:window.jiantDiagnostics.keys()})"),"true",js(activity,"window.jiantDiagnostics.peak>0.00001"));
   js(activity,"document.dispatchEvent(new KeyboardEvent('keyup',{code:'KeyZ',key:'z',bubbles:true}));document.dispatchEvent(new KeyboardEvent('keyup',{code:'KeyX',key:'x',bubbles:true}));");
   assertEquals("0",js(activity,"window.jiantDiagnostics.keys()"));
   screenshot(activity,p+"-portrait");
   js(activity,"document.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyZ',key:'z',bubbles:true}));");
   instrumentation.runOnMainSync(()->activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE));Thread.sleep(700);assertEquals(p,activity.bootedProfile);screenshot(activity,p+"-landscape");assertEquals("0",js(activity,"window.jiantDiagnostics.keys()"));
   instrumentation.runOnMainSync(()->activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT));Thread.sleep(700);assertEquals(p,activity.bootedProfile);
   instrumentation.runOnMainSync(activity::onBackPressed);Thread.sleep(1500);
  }
  instrumentation.runOnMainSync(activity::finish);
 }
}
