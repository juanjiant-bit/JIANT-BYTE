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
 protected String js(MainActivity a,String code)throws Exception {CountDownLatch done=new CountDownLatch(1);String[] result={null};InstrumentationRegistry.getInstrumentation().runOnMainSync(()->web(a.getWindow().getDecorView()).evaluateJavascript(code,r->{result[0]=r;done.countDown();}));assertTrue(done.await(5,TimeUnit.SECONDS));return result[0];}
 private void waitLayout(MainActivity a,String expected)throws Exception {long deadline=System.currentTimeMillis()+12000;String condition="window.jiantDiagnostics.layout()==='"+expected+"' && document.querySelector('#device').getBoundingClientRect().right<=innerWidth+1";while(!"true".equals(js(a,condition))&&System.currentTimeMillis()<deadline)Thread.sleep(200);assertEquals("Layout: "+js(a,"JSON.stringify({layout:window.jiantDiagnostics.layout(),width:innerWidth,height:innerHeight,panel:window.jiantDiagnostics.panel()})"),"true",js(a,condition));Thread.sleep(300);}
 protected void screenshot(MainActivity a,String name)throws Exception {android.content.ContentValues values=new android.content.ContentValues();values.put(android.provider.MediaStore.Images.Media.DISPLAY_NAME,name+".png");values.put(android.provider.MediaStore.Images.Media.MIME_TYPE,"image/png");values.put(android.provider.MediaStore.Images.Media.RELATIVE_PATH,"Pictures/JiantTest");android.net.Uri uri=a.getContentResolver().insert(android.provider.MediaStore.Images.Media.EXTERNAL_CONTENT_URI,values);assertNotNull(uri);try(OutputStream out=a.getContentResolver().openOutputStream(uri)){assertTrue(InstrumentationRegistry.getInstrumentation().getUiAutomation().takeScreenshot().compress(Bitmap.CompressFormat.PNG,100,out));}}

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
   waitLayout(activity,"tall");
   assertEquals(p+" visible portrait panel","true",js(activity,"window.jiantDiagnostics.panel().height>400 && document.querySelectorAll('.hit').length>=40"));
   screenshot(activity,p+"-portrait");
   js(activity,"document.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyZ',key:'z',bubbles:true}));");
   instrumentation.runOnMainSync(()->activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE));waitLayout(activity,"wide");assertEquals(p,activity.bootedProfile);assertEquals(p+" visible landscape panel","true",js(activity,"window.jiantDiagnostics.panel().height>100 && document.querySelectorAll('.hit').length>=40"));screenshot(activity,p+"-landscape");assertEquals("0",js(activity,"window.jiantDiagnostics.keys()"));
   instrumentation.runOnMainSync(()->activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT));waitLayout(activity,"tall");assertEquals(p,activity.bootedProfile);
   instrumentation.runOnMainSync(activity::onBackPressed);Thread.sleep(1500);
  }
  instrumentation.runOnMainSync(activity::finish);
 }
}
