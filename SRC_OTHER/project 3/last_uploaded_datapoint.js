const functions = require('@google-cloud/functions-framework');
const { Firestore } = require('@google-cloud/firestore');

const firestore = new Firestore();
const gfsCollectionName = process.env.COLLECTION_NAME || "sensor_data";

exports.getLatestDataPoint = async (req, res) => {
  try {
    const userId = req.query.userId || 'TimoID';

    console.log(`Checking path: ${gfsCollectionName}/${userId}/dataPoints`);

    const dataPointsRef = firestore
      .collection(gfsCollectionName)
      .doc(userId)
      .collection('dataPoints');

    // Order by timeCaptured inside otherDetails and get only the most recent one
    const querySnapshot = await dataPointsRef
      .orderBy('otherDetails.timeCaptured', 'desc')
      .limit(1)
      .get();

    if (querySnapshot.empty) {
      console.error(`No documents found in ${gfsCollectionName}/${userId}/dataPoints`);
      res.status(404).send(`No documents found for user ${userId}.`);
      return;
    }

    const doc = querySnapshot.docs[0];
    const data = doc.data();

    console.log(`Latest document ${doc.id}:\n`, JSON.stringify(data, null, 2));

    // Return only the latest document with its ID included
    res.status(200).json({ id: doc.id, ...data });
  } catch (error) {
    console.error('Error retrieving latest document:', error);
    res.status(500).send('Internal Server Error');
  }
};