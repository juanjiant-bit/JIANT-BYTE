package fm.jiant.emulator;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import org.junit.Test;
import org.junit.runner.RunWith;
import static org.junit.Assert.*;
@RunWith(AndroidJUnit4.class)
public class OfflineTest {
 @Test public void allBundledProfilesBootWithoutNetworkAndRotate()throws Exception{
  var instrumentation=InstrumentationRegistry.getInstrumentation();var context=instrumentation.getTargetContext();
  MainActivity activity=(MainActivity)instrumentation.startActivitySync(new Intent(context,MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
  for(String p:EngineStore.PROFILES){
   instrumentation.runOnMainSync(()->activity.openProfile(p));
   long deadline=System.currentTimeMillis()+25000;
   while(!p.equals(activity.bootedProfile)&&activity.bootError.isEmpty()&&System.currentTimeMillis()<deadline)Thread.sleep(100);
   assertEquals("Engine error: "+activity.bootError,p,activity.bootedProfile);
   instrumentation.runOnMainSync(()->activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE));Thread.sleep(700);assertEquals(p,activity.bootedProfile);
   instrumentation.runOnMainSync(()->activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT));Thread.sleep(700);assertEquals(p,activity.bootedProfile);
   instrumentation.runOnMainSync(activity::onBackPressed);Thread.sleep(1500);
  }
  instrumentation.runOnMainSync(activity::finish);
 }
}
