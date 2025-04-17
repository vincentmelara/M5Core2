// Imports
const functions = require('@google-cloud/functions-framework');
const {Firestore} = require('@google-cloud/firestore');
const {Storage} = require('@google-cloud/storage');
const fs = require("fs").promises; // File System


// TODO 3: Create GCS bucket and GFS collection and put their
// names in the cloud function's environment variables named
// below

// Function Variables
const gcsBucketName = process.env.BUCKET_NAME;
const gfsCollectionName = process.env.COLLECTION_NAME;

if (!gcsBucketName || !gfsCollectionName) {
    console.error("Missing required environment variables: BUCKET_NAME or COLLECTION_NAME");
    throw new Error("Environment variables BUCKET_NAME and COLLECTION_NAME must be set.");
}


// TODO 1: Create cloud function and ensure function entry
// point matches name passed into function/http definition.
//
// NOTE: To make an unauthenticated call, go to the cloud function,
// select "Permissions" and then "+Grant Access" for the "New 
// Principal" named "allUsers" and "Select a role" of "Cloud
// Functions Invoker"


// Function that is triggered when HTTP request is made
exports.vincent_cloud_function = async (req, res) => {

	// Determine if a file was uploaded or not
	const fileBuf = req.body; // The payload, which is the file uploaded via HTTP
	const hasFile = fileBuf && fileBuf.length > 0;
	

	// Get the M5-Details header (a String, but originally a JSON
    // object that was serialized to a String), which has M5Core2
    // data/details

	const strHeaderM5 = req.get('M5-Details');
	//console.log(`Header-M5: ${strHeaderM5}`);


	// Attempt to convert the string to a JSON object
	let objDetails;
	try {
		// Converting String to JSON object
		objDetails = JSON.parse(strHeaderM5);
		//console.log("objDetails:" + objDetails);

		// Check that JSON object has essential components
		if (!objDetails.hasOwnProperty("vcnlDetails") || !objDetails.hasOwnProperty("shtDetails") ||
			!objDetails.hasOwnProperty("m5Details") || !objDetails.hasOwnProperty("otherDetails")) {
				let message = 'Could NOT parse JSON details in header';
				console.error(message);
				res.status(400).send("Malformed request.");
			}
	} catch(e) {
		let message = 'Could NOT parse JSON details from header.';
		console.error(message);
		res.status(400).send("Malformed request.");
	}

	// TODO 4: Parse basic info from the HTTP custom header (JSON
	// object) and add additional info to header
	
	// Parse details from the header details (just proof of concept)
	const userId = objDetails.otherDetails.userId;
	const temp = objDetails.shtDetails.temp;
	const rHum = objDetails.shtDetails.rHum;
	const strWeatherSummary = `User: ${userId}, Temp: ${temp}, Hum: ${rHum}%`;
	console.log(strWeatherSummary);

	// Add the current time to the data
	const timeNow = Date.now();
	objDetails.otherDetails["cloudUploadTime"] = timeNow;


	// Creates Google Storage & Firestore Clients
	//
	// TODO 5: Go to IAM and add permissions for Firestore and
	// Cloud Storage for your cloud function's service account.

	const firestore = new Firestore();
	const storage = new Storage();

	// TODO 6: Write the details to Google Firestore
	let message = "";
	try {
		// Obtain a doc reference from the proper collection
		//const docRef = firestore.collection(gfsCollectionName).doc();
		const docRef = firestore.collection(gfsCollectionName)
			.doc(userId)
			.collection("dataPoints")
			.doc();  // Auto-generate document ID
		
		console.log(`Doc ref obtained: ${docRef.path}`);

		// Make entry into Firestore DB
		await docRef.set(objDetails);
		message = `GFS STATUS: M5 Details successfully uploaded to Firestore collection ${gfsCollectionName}`;
		console.log(message);
	} catch (e) {
		let eMessage = `GFS ERROR: Could NOT write details to ${gfsCollectionName}: ${e}`;
		res.status(400).send(message); // If failed, return now and don't attempt to upload file, even if one exists
	}

	// If make it this far, then GFS write was successful; if no
	// file was uploaded, just return success; if a file was uploaded
	// attempt to upload it to GCS.

	if (!hasFile) {
		// If no file was uploaded, then just use a GFS success as overall success and return the response
		res.status(200).send(message);
	} else {
	
		// TODO 7: Create new file name and filepath (for local (linux)
		// storage and google cloud storage). According to docs, only the
		// /tmp directory in Cloud Functions can be written to.
		// https://cloud.google.com/functions/docs/concepts/exec#file_system
	
		const fileName = `${userId}_${timeNow}.txt`;
		const fsFileName = `/tmp/${fileName}`;
		const gsFileName = `users/${userId}/${fileName}`;
		
	
		// Write file to local file system (really just in memory) for
		// upload to Google Storage
	
		// Option 1: Just create dummy file as proof of concept
		// fs.writeFile(fsFileName, strHeaderM5, function (err) {
		// 	if (err) throw err;
		// 	console.log(`${fsFileName} created successfully.`);
		// });

		// TODO 9: Option 2: Write file passed as HTTP payload
		console.log(`File size received from buffer: ${fileBuf.length} bytes.`);
		fs.writeFile(fsFileName, fileBuf, function (err) {
			if (err) throw err;
			console.log(`${fsFileName} created successfully.`);
		});

		// TODO 8: Upload a local file to the GCS bucket
		try {
			// Upload file to GCS
			await storage.bucket(gcsBucketName).upload(fsFileName, 
				{
					destination: gsFileName, 	// Where to save in GCS
					// gzip: true,				// Compress before sending
					metadata: {
						cacheControl: 'public, max-age=31536000', // Contents will never change so cache
					},
				}
			);

			// The upload was successful, so log and return the result
			let message2 = `${fileName} successfully uploaded to Google Storage bucket ${gcsBucketName}.`;
			console.log(message2);
			res.status(200).send(message + "\n" + message2);
		} catch (e) {
			let eMessage = `GCS ERROR: Could NOT upload ${fileName} to ${gcsBucketName}: ${e}`;
			console.log(eMessage);
			res.status(400).send(eMessage);
		}
		
		// If made it here, we did not have a successful run; return
		// a generic error message to the client.
	
		//res.status(400).send("There was an exception that occurred during runtime.");
	}

};

/*
// "Pretty" JSON Model Example
{
	"vcnlDetails":
	{
		"prox": 100,
		"rwl": 200,
		"als": 300
	},
	"shtDetails":
	{
		"temp": 72.5,
		"rHum": 55.6
	},
	"m5Details":
	{
		"ax": -0.5,
		"ay": 0.0,
		"az": 0.5
	},
	"otherDetails":
	{
		"userId": "DrDan",
		"captureTime": 1648591655046
	}
}

// "Minimized" JSON Model Example
{"vcnlDetails":{"prox": 100,"rwl": 200,	"als": 300},"shtDetails":{"temp": 72.5,"rHum": 55.6},"m5Details":{"ax": -0.5,"ay": 0.0,"az": 0.5},"otherDetails":{"userId": "DrDan","captureTime": 1648591655046}}
*/



