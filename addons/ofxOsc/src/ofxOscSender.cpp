// copyright (c) openFrameworks team 2010-2017
// copyright (c) Damian Stewart 2007-2009
#include "ofxOscSender.h"
#include "ofUtils.h"
#include "ofParameterGroup.h"

#include "UdpSocket.h"

//--------------------------------------------------------------
ofxOscSender::ofxOscSender() : broadcast(true), host(""), port(0) {}

//--------------------------------------------------------------
ofxOscSender::ofxOscSender(const ofxOscSender & mom) : broadcast(mom.broadcast){
	copy(mom);
}

//--------------------------------------------------------------
ofxOscSender& ofxOscSender::operator=(const ofxOscSender & mom){
	return copy(mom);
}

//--------------------------------------------------------------
ofxOscSender& ofxOscSender::copy(const ofxOscSender& other){
	if(this == &other) return *this;
	broadcast = other.broadcast;
	host = other.host;
	port = other.port;
	if(other.sendSocket){
		setup(host, port);
	}
	return *this;
}

//--------------------------------------------------------------
void ofxOscSender::setup(const string &host, int port){
	if(osc::UdpSocket::GetUdpBufferSize() == 0){
	   osc::UdpSocket::SetUdpBufferSize(65535);
	}
	sendSocket.reset(new osc::UdpTransmitSocket(osc::IpEndpointName(host.c_str(), port), broadcast));
	this->host = host;
	this->port = port;
}

//--------------------------------------------------------------
void ofxOscSender::clear(){
	sendSocket.reset();
}

//--------------------------------------------------------------
void ofxOscSender::sendBundle(const ofxOscBundle& bundle){
	if(!sendSocket){
		ofLogError("ofxOscSender") << "trying to send before setup";
	}
	
	// setting this much larger as it gets trimmed down to the size its using before being sent.
	// TODO: much better if we could make this dynamic? Maybe have ofxOscBundle return its size?
	static const int OUTPUT_BUFFER_SIZE = 327680;
	char buffer[OUTPUT_BUFFER_SIZE];
	osc::OutboundPacketStream p(buffer, OUTPUT_BUFFER_SIZE);

	// serialise the bundle and send
	appendBundle(bundle, p);
	sendSocket->Send(p.Data(), p.Size());
}

//--------------------------------------------------------------
void ofxOscSender::sendMessage(const ofxOscMessage& message, bool wrapInBundle){
	if(!sendSocket){
		ofLogError("ofxOscSender") << "trying to send before setup";
	}
	
	// setting this much larger as it gets trimmed down to the size its using before being sent.
	// TODO: much better if we could make this dynamic? Maybe have ofxOscMessage return its size?
	static const int OUTPUT_BUFFER_SIZE = 327680;
	char buffer[OUTPUT_BUFFER_SIZE];
	osc::OutboundPacketStream p(buffer, OUTPUT_BUFFER_SIZE);

	// serialise the message and send
	if(wrapInBundle) {
		p << osc::BeginBundleImmediate;
	}
	appendMessage(message, p);
	if(wrapInBundle) {
		p << osc::EndBundle;
	}
	sendSocket->Send(p.Data(), p.Size());
}

//--------------------------------------------------------------
void ofxOscSender::sendParameter(const ofAbstractParameter & parameter){
	if(!parameter.isSerializable()) return;
	if(parameter.type() == typeid(ofParameterGroup).name()){
		string address = "/";
		const vector<string> hierarchy = parameter.getGroupHierarchyNames();
		for(int i = 0; i < (int)hierarchy.size()-1; i++){
			address += hierarchy[i] + "/";
		}
		ofxOscBundle bundle;
		appendParameter(bundle, parameter, address);
		sendBundle(bundle);
	}
	else{
		string address = "";
		const vector<string> hierarchy = parameter.getGroupHierarchyNames();
		for(int i = 0; i < (int)hierarchy.size()-1; i++){
			address += "/" + hierarchy[i];
		}
		if(address.length()) {
			address += "/";
		}
		ofxOscMessage msg;
		appendParameter(msg, parameter, address);
		sendMessage(msg, false);
	}
}

//--------------------------------------------------------------
string ofxOscSender::getHost(){
	return host;
}

//--------------------------------------------------------------
int ofxOscSender::getPort(){
	return port;
}

//--------------------------------------------------------------
void ofxOscSender::disableBroadcast(){
	broadcast = false;
	if(sendSocket){
		setup(host, port);
	}
}

//--------------------------------------------------------------
void ofxOscSender::enableBroadcast(){
	broadcast = true;
	if(sendSocket){
		setup(host, port);
	}
}

// PRIVATE
//--------------------------------------------------------------
void ofxOscSender::appendBundle(const ofxOscBundle& bundle, osc::OutboundPacketStream& p){
	// recursively serialise the bundle
	p << osc::BeginBundleImmediate;
	for(int i = 0; i < bundle.getBundleCount(); i++){
		appendBundle(bundle.getBundleAt(i), p);
	}
	for(int i = 0; i < bundle.getMessageCount(); i++){
		appendMessage(bundle.getMessageAt(i), p);
	}
	p << osc::EndBundle;
}

//--------------------------------------------------------------
void ofxOscSender::appendMessage(const ofxOscMessage& message, osc::OutboundPacketStream& p){
	p << osc::BeginMessage(message.getAddress().c_str());
	for(int i = 0; i < message.getNumArgs(); ++i) {
		switch(message.getArgType(i)){
			case OFXOSC_TYPE_INT32:
				p << message.getArgAsInt32(i);
				break;
			case OFXOSC_TYPE_INT64:
				p << (osc::int64)message.getArgAsInt64(i);
				break;
			case OFXOSC_TYPE_FLOAT:
				p << message.getArgAsFloat(i);
				break;
			case OFXOSC_TYPE_DOUBLE:
				p << message.getArgAsDouble(i);
				break;
			case OFXOSC_TYPE_STRING:
				p << message.getArgAsString(i).c_str();
				break;
			case OFXOSC_TYPE_SYMBOL:
				p << osc::Symbol(message.getArgAsString(i).c_str());
				break;
			case OFXOSC_TYPE_CHAR:
				p << message.getArgAsChar(i);
				break;
			case OFXOSC_TYPE_MIDI_MESSAGE:
				p << osc::MidiMessage(message.getArgAsMidiMessage(i));
				break;
			case OFXOSC_TYPE_TRUE: case OFXOSC_TYPE_FALSE:
				p << message.getArgAsBool(i);
				break;
			case OFXOSC_TYPE_NONE:
				p << osc::NilType();
				break;
			case OFXOSC_TYPE_TRIGGER:
				p << osc::InfinitumType();
				break;
			case OFXOSC_TYPE_TIMETAG:
				p << osc::TimeTag(message.getArgAsTimetag(i));
				break;
			case OFXOSC_TYPE_RGBA_COLOR:
				p << osc::RgbaColor(message.getArgAsRgbaColor(i));
				break;
			case OFXOSC_TYPE_BLOB: {
				ofBuffer buff = message.getArgAsBlob(i);
				p << osc::Blob(buff.getData(), (unsigned long)buff.size());
				break;
			}
			default:
				ofLogError("ofxOscSender") << "appendMessage(): bad argument type "
					<< message.getArgType(i);
				assert(false);
				break;
		}
	}
	p << osc::EndMessage;
}

//--------------------------------------------------------------
void ofxOscSender::appendParameter(ofxOscBundle & _bundle, const ofAbstractParameter & parameter, const string &address){
	if(parameter.type() == typeid(ofParameterGroup).name()){
		ofxOscBundle bundle;
		const ofParameterGroup & group = static_cast<const ofParameterGroup &>(parameter);
		for(size_t i = 0; i < group.size(); i++){
			const ofAbstractParameter & p = group[i];
			if(p.isSerializable()){
				appendParameter(bundle, p, address+group.getEscapedName()+"/");
			}
		}
		_bundle.addBundle(bundle);
	}
	else{
		if(parameter.isSerializable()){
			ofxOscMessage msg;
			appendParameter(msg, parameter, address);
			_bundle.addMessage(msg);
		}
	}
}

//--------------------------------------------------------------
void ofxOscSender::appendParameter(ofxOscMessage & msg, const ofAbstractParameter & parameter, const string &address){
	msg.setAddress(address+parameter.getEscapedName());
	if(parameter.type() == typeid(ofParameter<int>).name()){
		msg.addIntArg(parameter.cast<int>());
	}
	else if(parameter.type() == typeid(ofParameter<float>).name()){
		msg.addFloatArg(parameter.cast<float>());
	}
	else if(parameter.type() == typeid(ofParameter<double>).name()){
		msg.addDoubleArg(parameter.cast<double>());
	}
	else if(parameter.type() == typeid(ofParameter<bool>).name()){
		msg.addBoolArg(parameter.cast<bool>());
	}
	else{
		msg.addStringArg(parameter.toString());
	}
}
