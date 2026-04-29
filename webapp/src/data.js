import { collection, query, where, orderBy, getDocs, Timestamp } from 'firebase/firestore'
import { db, COLLECTION } from './firebase.js'

export async function fetchSamples(hours) {
  const since = new Date(Date.now() - hours * 3600 * 1000)
  const isoSince = since.toISOString().replace('.000', '')

  const q = query(
    collection(db, COLLECTION),
    where('timestamp', '>=', isoSince),
    orderBy('timestamp', 'asc'),
  )

  const snap = await getDocs(q)
  return snap.docs.map(d => d.data())
}
