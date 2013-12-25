package com.webmajstr.pebble_gc;

import java.util.UUID;

import com.getpebble.android.kit.PebbleKit;
import com.getpebble.android.kit.util.PebbleDictionary;


import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Bundle;
import android.os.IBinder;
import android.support.v4.app.NotificationCompat;
import android.widget.Toast;

public class WatchService extends Service {
	
	LocationManager locationManager;
    LocationListener locationListener;
    
    PendingIntent contentIntent;
    
    Location geocacheLocation = new Location("");
    
    private UUID uuid = UUID.fromString("6191ad65-6cb1-404f-bccc-2446654c20ab"); //v2
    
    @Override
    public IBinder onBind(Intent arg0) {
          return null;
    }
    
    @Override
    public void onCreate() {
    	super.onCreate();

    	// Listen when notification is clicked to close the service
        IntentFilter filter = new IntentFilter("android.intent.CLOSE_ACTIVITY");
		registerReceiver(mReceiver, filter);
    	
    	//set to run in foreground, so it's not killed by android
		showNotification();		
					
		//register for GPS location updates
		registerLocationUpdates();
		
        startWatchApp();
	
    }
    
    public void stopApp(){
    	this.stopSelf();
    }
    
    BroadcastReceiver mReceiver = new BroadcastReceiver() {

	    @Override
	    public void onReceive(Context context, Intent intent) {
	    	
	        if(intent.getAction().equals("android.intent.CLOSE_ACTIVITY")){
	    		stopApp();
	    	}
	    	
	    }

	};
    
    void registerLocationUpdates(){
    	// Acquire a reference to the system Location Manager
        locationManager = (LocationManager) this.getSystemService(Context.LOCATION_SERVICE);

        // Define a listener that 'responds' to location updates
        locationListener = new LocationListener() {
            public void onLocationChanged(Location location) {
            	locationUpdate(location);
            }

            public void onStatusChanged(String provider, int status, Bundle extras) {}

            public void onProviderEnabled(String provider) {}

            public void onProviderDisabled(String provider) {}
          };

        // Register the listener with the Location Manager to receive location updates
        // minTime is set to 0 to keep GPS on with most mobile phones, although value like 2000 seems OK for most of them
        // impact on battery has not been determined
        locationManager.requestLocationUpdates(LocationManager.GPS_PROVIDER, 0, 1, locationListener);
      	
    }
    
    void locationUpdate(Location currentLocation){

    	if(!currentLocation.getProvider().equals("gps")) return;
    	
    	
    	float distance = currentLocation.distanceTo(geocacheLocation);
    	float bearing = currentLocation.getBearing();
    	float azimut = currentLocation.bearingTo(geocacheLocation);
    	if(azimut < 0) azimut = 360 + azimut;
    	if(bearing < 0) bearing = 360 + bearing;

    	float direction = azimut - bearing;
		if(direction < 0) direction = 360 + direction;
		
		updateWatch(distance, direction);
    
    }
    
    public void startWatchApp() {
        PebbleKit.startAppOnPebble(getApplicationContext(), uuid);
    }

    public void stopWatchApp() {
        PebbleKit.closeAppOnPebble(getApplicationContext(), uuid);
    }
    
    
    public void updateWatch(float distance, float azimut) {

    	int azimutInt = (int)Math.round(azimut);
    	int distanceInt = (int)Math.round(distance);
    	
    	// convert azimut in degrees to index of image to show. north +- 15 degrees is index 0,
    	// azimut of 30 degrees +- 15 degrees is index 1, etc..
    	int azimutIndex = ((azimutInt + 15)/30) % 12;
    	
        PebbleDictionary data = new PebbleDictionary();

        data.addString(0, String.format("%d m", distanceInt));
        data.addUint8(1, (byte)azimutIndex);
        
        PebbleKit.sendDataToPebble(getApplicationContext(), uuid, data);
    }
    
    public void updateWatchInit() {

    	PebbleDictionary data = new PebbleDictionary();

        data.addString(0, "Starting..");
        data.addUint8(1, (byte)0);
        
        PebbleKit.sendDataToPebble(getApplicationContext(), uuid, data);
    }
    
    private void showNotification() {
    	
        Intent notificationIntent = new Intent("android.intent.CLOSE_ACTIVITY");
        PendingIntent intent = PendingIntent.getBroadcast(this, 0 , notificationIntent, 0);
    	
    	Notification notification =
    		    new NotificationCompat.Builder(getApplicationContext())
    		    .setSmallIcon(R.drawable.ic_launcher)
    		    .setContentTitle(getText(R.string.app_name))
    		    .setContentText(getText(R.string.service_started))
    		    .setContentIntent(intent)
    		    .build();
    	
    	// actually start foreground activity
    	startForeground(R.string.service_started, notification);

    } 
    
    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {

    	//reset watch to default state
    	updateWatchInit();
    	
		Toast.makeText(this, R.string.navigation_has_started, Toast.LENGTH_LONG).show();
    	
    	Double gc_latitude = intent.getDoubleExtra("latitude", 0.0);
    	Double gc_longitude = intent.getDoubleExtra("longitude", 0.0);
        geocacheLocation.setLatitude( gc_latitude );
        geocacheLocation.setLongitude( gc_longitude );
                
        // We want this service to continue running until it is explicitly
        // stopped, so return sticky.
        return START_STICKY;
    }
    
    @Override
    public void onDestroy() {
    	
    	stopWatchApp();
    	
    	unregisterReceiver(mReceiver);
    	
    	// stop listening for GPS updates
		locationManager.removeUpdates(locationListener);
		  
		// stop foreground
		stopForeground(true);

		super.onDestroy();
    }
    
    
}