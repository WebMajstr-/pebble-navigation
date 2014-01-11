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
import android.util.Log;
import android.widget.Toast;

public class WatchService extends Service {
	
	LocationManager locationManager;
    LocationListener locationListener;
    
    PendingIntent contentIntent;
    
    Location geocacheLocation = new Location("");
    
    float gc_difficulty, gc_terrain;
	String gc_name, gc_code, gc_size;
	
	private static final int DISTANCE_KEY = 0;
	private static final int AZIMUT_INDEX_KEY = 1;
	private static final int EXTRAS_KEY = 2;
	private static final int DT_RATING_KEY = 3;
	private static final int GC_NAME_KEY = 4;
	private static final int GC_CODE_KEY = 5;
	private static final int GC_SIZE_KEY = 6;
	
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
        // impact on battery has not been determined
        locationManager.requestLocationUpdates(LocationManager.GPS_PROVIDER, 2000, 1, locationListener);
      	
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
		
		updateWatchWithLocation(distance, direction);
    
    }
    
    public void startWatchApp() {
        PebbleKit.startAppOnPebble(getApplicationContext(), uuid);
    }

    public void stopWatchApp() {
        PebbleKit.closeAppOnPebble(getApplicationContext(), uuid);
    }
    
    
    public void updateWatchWithLocation(float distance, float azimut) {

    	int azimutInt = (int)Math.round(azimut);
    	int distanceInt = (int)Math.round(distance);
    	
    	// convert azimut in degrees to index of image to show. north +- 15 degrees is index 0,
    	// azimut of 30 degrees +- 15 degrees is index 1, etc..
    	int azimutIndex = ((azimutInt + 15)/30) % 12;
    	
    	sendToPebble(String.format("%d m", distanceInt), azimutIndex);
    	
    }
    
    public void sendToPebble(String distance, int azimutIndex) {
    	
    	boolean hasExtras = checkHasExtras();
    	
    	PebbleDictionary data = new PebbleDictionary();

        data.addString(DISTANCE_KEY, distance);
        data.addUint8(AZIMUT_INDEX_KEY, (byte)azimutIndex);
        data.addUint8(EXTRAS_KEY, (byte)(hasExtras?1:0) );
        
        if(hasExtras){
        	
        	data.addString(DT_RATING_KEY, String.format("D%f/T%f", gc_difficulty, gc_terrain));
        	data.addString(GC_NAME_KEY, gc_name.substring(0, 20));
        	data.addString(GC_CODE_KEY, gc_code);
        	data.addString(GC_SIZE_KEY, gc_size);
        	
        }
        
        PebbleKit.sendDataToPebble(getApplicationContext(), uuid, data);
        
    }
    
    private boolean checkHasExtras() {

    	// function that makes sure that extras exists.
    	// Not really smart way, but works
    	if(gc_name == null || gc_code == null || gc_size == null){
    		return false;
    	} else {
    		return true;
    	}
    	
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
    	sendToPebble("NO GPS", 0);
    	
		Toast.makeText(this, R.string.navigation_has_started, Toast.LENGTH_LONG).show();
    	
    	Double gc_latitude = intent.getDoubleExtra("latitude", 0.0);
    	Double gc_longitude = intent.getDoubleExtra("longitude", 0.0);
    	
    	gc_difficulty = intent.getFloatExtra("difficulty", 0);
		gc_terrain = intent.getFloatExtra("terrain", 0);
		gc_name = intent.getStringExtra("name");
		gc_code = intent.getStringExtra("code");
		gc_size = intent.getStringExtra("size");
		
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