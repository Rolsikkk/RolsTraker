const express = require('express');
const app = express();
app.use(express.json());
app.use(express.static('public'));

let matchState = { phase: "none", players: [] };

app.post('/api/update', (req, res) => {
    // You could add a secret key check here in the future
    matchState = req.body;
    res.json({ success: true });
});

app.get('/api/game', (req, res) => {
    res.json(matchState);
});

app.listen(3000, () => {
    console.log('Server is running on port 3000');
});
