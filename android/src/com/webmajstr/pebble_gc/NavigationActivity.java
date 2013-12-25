package com.webmajstr.pebble_gc;

import android.os.Bundle;
import android.app.Activity;
import android.content.Intent;

public class NavigationActivity extends Activity {

	// Variables that I receive from c:geo as Intent
	double gc_longitude, gc_latitude;
	float gc_difficulty, gc_terrain;
	String gc_name, gc_code, gc_size;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_navigation);

		Intent intent = getIntent();
		gc_latitude = intent.getDoubleExtra("latitude", 0.0);
		gc_longitude = intent.getDoubleExtra("longitude", 0.0);
		gc_difficulty = intent.getFloatExtra("difficulty", 0);
		gc_terrain = intent.getFloatExtra("terrain", 0);
		gc_name = intent.getStringExtra("name");
		gc_code = intent.getStringExtra("code");
		gc_size = intent.getStringExtra("size");
		
		startWatchService();
		finish();

	}

	public void startWatchService() {
		Intent intent = new Intent(NavigationActivity.this, WatchService.class);
		intent.putExtra("latitude", gc_latitude);
		intent.putExtra("longitude", gc_longitude);
		
		intent.putExtra("difficulty", gc_difficulty);
		intent.putExtra("terrain", gc_terrain);
		intent.putExtra("name", gc_name);
		intent.putExtra("code", gc_code);
		intent.putExtra("size", gc_size);
		
		startService(intent);
	}

}
